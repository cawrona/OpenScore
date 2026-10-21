// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AnnotationStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtMath>

namespace {

qreal distSqToSegment(QPointF p, QPointF a, QPointF b)
{
    const QPointF ab = b - a;
    const qreal len2 = ab.x() * ab.x() + ab.y() * ab.y();
    qreal t = 0;
    if (len2 > 0)
        t = qBound(0.0, ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2, 1.0);
    const QPointF c = a + ab * t;
    const QPointF d = p - c;
    return d.x() * d.x() + d.y() * d.y();
}

QJsonObject strokeToJson(const Stroke &s)
{
    QJsonArray flat;
    for (int i = 0; i < s.pts.size(); ++i) {
        flat.append(qRound(s.pts[i].x() * 100000) / 100000.0);
        flat.append(qRound(s.pts[i].y() * 100000) / 100000.0);
        flat.append(qRound(s.pressure.value(i, 1.0f) * 1000) / 1000.0);
    }
    return QJsonObject{
        {QStringLiteral("tool"), s.tool == Stroke::Pen ? QStringLiteral("pen") : QStringLiteral("highlighter")},
        {QStringLiteral("color"), s.color.name(QColor::HexArgb)},
        {QStringLiteral("width"), double(s.width)},
        {QStringLiteral("pts"), flat},
    };
}

Stroke strokeFromJson(const QJsonObject &o)
{
    Stroke s;
    s.tool = o.value(QStringLiteral("tool")).toString() == QLatin1String("highlighter") ? Stroke::Highlighter : Stroke::Pen;
    s.color = QColor(o.value(QStringLiteral("color")).toString());
    s.width = float(o.value(QStringLiteral("width")).toDouble(0.002));
    const QJsonArray flat = o.value(QStringLiteral("pts")).toArray();
    for (int i = 0; i + 2 < flat.size(); i += 3) {
        s.pts.push_back(QPointF(flat[i].toDouble(), flat[i + 1].toDouble()));
        s.pressure.push_back(float(flat[i + 2].toDouble(1.0)));
    }
    return s;
}

} // namespace

AnnotationStore::AnnotationStore(QObject *parent)
    : QObject(parent)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(800);
    connect(&m_saveTimer, &QTimer::timeout, this, &AnnotationStore::flush);
}

AnnotationStore::~AnnotationStore()
{
    flush();
}

QString AnnotationStore::filePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/annotations/") + m_sha1
        + QStringLiteral(".json");
}

void AnnotationStore::load(const QString &sha1)
{
    flush();
    m_sha1 = sha1;
    m_pages.clear();
    m_undo.clear();
    m_redo.clear();
    m_dirty = false;
    if (!sha1.isEmpty()) {
        QFile f(filePath());
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject pages = QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("pages")).toObject();
            for (auto it = pages.begin(); it != pages.end(); ++it) {
                QVector<Stroke> v;
                for (const QJsonValue &sv : it.value().toArray())
                    v.push_back(strokeFromJson(sv.toObject()));
                m_pages.insert(it.key().toInt(), v);
            }
        }
    }
    emit historyChanged();
    emit changed(-1);
}

void AnnotationStore::flush()
{
    m_saveTimer.stop();
    if (!m_dirty || m_sha1.isEmpty())
        return;
    QJsonObject pages;
    for (auto it = m_pages.cbegin(); it != m_pages.cend(); ++it) {
        if (it.value().isEmpty())
            continue;
        QJsonArray arr;
        for (const Stroke &s : it.value())
            arr.append(strokeToJson(s));
        pages.insert(QString::number(it.key()), arr);
    }
    QDir().mkpath(QFileInfo(filePath()).absolutePath());
    QSaveFile f(filePath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), 1}, {QStringLiteral("pages"), pages}}).toJson(QJsonDocument::Compact));
        if (f.commit())
            m_dirty = false;
    }
}

void AnnotationStore::markDirty()
{
    m_dirty = true;
    m_saveTimer.start();
}

const QVector<Stroke> &AnnotationStore::strokes(int page) const
{
    static const QVector<Stroke> none;
    const auto it = m_pages.constFind(page);
    return it == m_pages.constEnd() ? none : it.value();
}

void AnnotationStore::addStroke(int page, const Stroke &s)
{
    QVector<Stroke> &v = m_pages[page];
    Op op;
    op.kind = Op::Add;
    op.page = page;
    op.items.push_back({int(v.size()), s});
    v.push_back(s);
    m_undo.push_back(op);
    m_redo.clear();
    markDirty();
    emit historyChanged();
    emit changed(page);
}

void AnnotationStore::beginErase()
{
    m_erase = Op();
    m_erase.kind = Op::Erase;
    m_erasing = true;
}

int AnnotationStore::eraseAt(int page, QPointF n, qreal radius, qreal aspect)
{
    if (!m_erasing)
        return 0;
    auto it = m_pages.find(page);
    if (it == m_pages.end())
        return 0;
    QVector<Stroke> &v = it.value();
    int removed = 0;
    const QPointF p(n.x(), n.y() * aspect);
    for (int i = int(v.size()) - 1; i >= 0; --i) {
        const Stroke &s = v[i];
        const qreal r = radius + s.width * 0.5;
        const qreal r2 = r * r;
        bool hit = false;
        for (int k = 0; k < s.pts.size() && !hit; ++k) {
            const QPointF a(s.pts[k].x(), s.pts[k].y() * aspect);
            const QPointF b = k + 1 < s.pts.size() ? QPointF(s.pts[k + 1].x(), s.pts[k + 1].y() * aspect) : a;
            hit = distSqToSegment(p, a, b) <= r2;
        }
        if (hit) {
            m_erase.page = page;
            m_erase.items.push_back({i, s});
            v.removeAt(i);
            ++removed;
        }
    }
    if (removed) {
        markDirty();
        emit changed(page);
    }
    return removed;
}

void AnnotationStore::endErase()
{
    if (!m_erasing)
        return;
    m_erasing = false;
    if (!m_erase.items.isEmpty()) {
        m_undo.push_back(m_erase);
        m_redo.clear();
        emit historyChanged();
    }
    m_erase = Op();
}

void AnnotationStore::clearPage(int page)
{
    const QVector<Stroke> v = strokes(page);
    if (v.isEmpty())
        return;
    beginErase();
    m_erase.page = page;
    for (int i = int(v.size()) - 1; i >= 0; --i)
        m_erase.items.push_back({i, v[i]});
    m_pages[page].clear();
    endErase();
    markDirty();
    emit changed(page);
}

void AnnotationStore::applyUndo(const Op &op)
{
    QVector<Stroke> &v = m_pages[op.page];
    if (op.kind == Op::Add) {
        for (int i = int(op.items.size()) - 1; i >= 0; --i)
            v.removeAt(qMin(op.items[i].first, int(v.size()) - 1));
    } else {
        for (int i = int(op.items.size()) - 1; i >= 0; --i)
            v.insert(qMin(op.items[i].first, int(v.size())), op.items[i].second);
    }
}

void AnnotationStore::applyRedo(const Op &op)
{
    QVector<Stroke> &v = m_pages[op.page];
    if (op.kind == Op::Add) {
        for (const auto &it : op.items)
            v.insert(qMin(it.first, int(v.size())), it.second);
    } else {
        for (const auto &it : op.items)
            v.removeAt(qMin(it.first, int(v.size()) - 1));
    }
}

void AnnotationStore::undo()
{
    if (m_undo.isEmpty())
        return;
    const Op op = m_undo.takeLast();
    applyUndo(op);
    m_redo.push_back(op);
    markDirty();
    emit historyChanged();
    emit changed(op.page);
}

void AnnotationStore::redo()
{
    if (m_redo.isEmpty())
        return;
    const Op op = m_redo.takeLast();
    applyRedo(op);
    m_undo.push_back(op);
    markDirty();
    emit historyChanged();
    emit changed(op.page);
}
