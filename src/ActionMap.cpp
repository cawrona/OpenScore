// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ActionMap.h"

#include <QKeySequence>
#include <QSettings>
#include <Qt>

namespace {

struct ActionInfo {
    const char *id;
    const char *label;
};

const ActionInfo kActions[] = {
    {"next", "Next page"},
    {"prev", "Previous page"},
    {"first", "First page"},
    {"last", "Last page"},
    {"undo", "Undo ink"},
    {"redo", "Redo ink"},
    {"twoUp", "Toggle two pages"},
};

const Qt::KeyboardModifiers kModMask = Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier;

QString keyBinding(int key, int modifiers)
{
    return QStringLiteral("key:%1:%2").arg(key).arg(modifiers & int(kModMask));
}

bool isModifierKey(int key)
{
    return key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt || key == Qt::Key_Meta
        || key == Qt::Key_AltGr || key == Qt::Key_CapsLock;
}

QHash<QString, QStringList> defaults()
{
    QHash<QString, QStringList> d;
    // Page turners usually send arrows or PageUp/PageDown; keep all the common ones.
    d[QStringLiteral("next")] = {keyBinding(Qt::Key_Right, 0), keyBinding(Qt::Key_Down, 0), keyBinding(Qt::Key_PageDown, 0),
                                 keyBinding(Qt::Key_Space, 0)};
    d[QStringLiteral("prev")] = {keyBinding(Qt::Key_Left, 0), keyBinding(Qt::Key_Up, 0), keyBinding(Qt::Key_PageUp, 0),
                                 keyBinding(Qt::Key_Backspace, 0)};
    d[QStringLiteral("first")] = {keyBinding(Qt::Key_Home, 0)};
    d[QStringLiteral("last")] = {keyBinding(Qt::Key_End, 0)};
    d[QStringLiteral("undo")] = {keyBinding(Qt::Key_Z, Qt::ControlModifier)};
    d[QStringLiteral("redo")] = {keyBinding(Qt::Key_Z, Qt::ControlModifier | Qt::ShiftModifier), keyBinding(Qt::Key_Y, Qt::ControlModifier)};
    d[QStringLiteral("twoUp")] = {keyBinding(Qt::Key_2, 0)};
    return d;
}

} // namespace

ActionMap::ActionMap(QObject *parent)
    : QObject(parent)
{
    load();
}

QVariantList ActionMap::actions() const
{
    QVariantList out;
    for (const ActionInfo &a : kActions)
        out.push_back(QVariantMap{{QStringLiteral("id"), QString::fromLatin1(a.id)}, {QStringLiteral("label"), QString::fromLatin1(a.label)}});
    return out;
}

void ActionMap::load()
{
    QSettings s;
    m_bindings = defaults();
    s.beginGroup(QStringLiteral("bindings"));
    for (const ActionInfo &a : kActions) {
        const QString id = QString::fromLatin1(a.id);
        if (s.contains(id))
            m_bindings[id] = s.value(id).toStringList();
    }
    s.endGroup();
}

void ActionMap::save() const
{
    QSettings s;
    s.beginGroup(QStringLiteral("bindings"));
    for (const ActionInfo &a : kActions) {
        const QString id = QString::fromLatin1(a.id);
        s.setValue(id, m_bindings.value(id));
    }
    s.endGroup();
}

QString ActionMap::label(const QString &binding)
{
    const QStringList p = binding.split(QLatin1Char(':'));
    if (p.value(0) == QLatin1String("key") && p.size() == 3)
        return QKeySequence(p[1].toInt() | p[2].toInt()).toString(QKeySequence::NativeText);
    if (p.value(0) == QLatin1String("midi") && p.size() == 3) {
        const QString kind = p[1] == QLatin1String("cc") ? QStringLiteral("CC") : p[1] == QLatin1String("pc") ? QStringLiteral("Program") : QStringLiteral("Note");
        return QStringLiteral("MIDI %1 %2").arg(kind, p[2]);
    }
    return binding;
}

QStringList ActionMap::bindingLabels(const QString &action) const
{
    QStringList out;
    for (const QString &b : m_bindings.value(action))
        out << label(b);
    return out;
}

void ActionMap::learn(const QString &action)
{
    m_learning = action;
    emit learningChanged();
}

void ActionMap::cancelLearn()
{
    if (m_learning.isEmpty())
        return;
    m_learning.clear();
    emit learningChanged();
}

void ActionMap::clearBindings(const QString &action)
{
    m_bindings[action].clear();
    save();
    ++m_revision;
    emit bindingsChanged();
}

void ActionMap::resetDefaults()
{
    m_bindings = defaults();
    save();
    ++m_revision;
    emit bindingsChanged();
}

void ActionMap::addBinding(const QString &action, const QString &binding)
{
    // One input does one thing: steal it from any other action.
    for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it)
        it.value().removeAll(binding);
    m_bindings[action].push_back(binding);
    save();
    ++m_revision;
    emit bindingsChanged();
}

void ActionMap::dispatch(const QString &binding)
{
    if (!m_learning.isEmpty()) {
        const QString action = m_learning;
        m_learning.clear();
        addBinding(action, binding);
        emit learningChanged();
        return;
    }
    for (auto it = m_bindings.cbegin(); it != m_bindings.cend(); ++it) {
        if (it.value().contains(binding)) {
            emit triggered(it.key());
            return;
        }
    }
}

bool ActionMap::handleKey(int key, int modifiers)
{
    if (isModifierKey(key))
        return false;
    if (!m_learning.isEmpty() && key == Qt::Key_Escape) {
        cancelLearn();
        return true;
    }
    const QString b = keyBinding(key, modifiers);
    if (!m_learning.isEmpty()) {
        dispatch(b);
        return true;
    }
    for (auto it = m_bindings.cbegin(); it != m_bindings.cend(); ++it) {
        if (it.value().contains(b)) {
            emit triggered(it.key());
            return true;
        }
    }
    return false;
}

void ActionMap::handleMidi(int status, int data1, int data2)
{
    const int kind = status & 0xF0;
    if (kind == 0xB0) { // control change: pedals send 127 down, 0 up
        const bool down = data2 >= 64;
        const bool wasDown = m_ccState.value(data1, 0) >= 64;
        m_ccState[data1] = data2;
        if (down && !wasDown)
            dispatch(QStringLiteral("midi:cc:%1").arg(data1));
    } else if (kind == 0xC0) { // program change
        dispatch(QStringLiteral("midi:pc:%1").arg(data1));
    } else if (kind == 0x90 && data2 > 0) { // note on
        dispatch(QStringLiteral("midi:note:%1").arg(data1));
    }
}
