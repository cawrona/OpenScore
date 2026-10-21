// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>

// Maps physical inputs (keyboard keys, which is how nearly every Bluetooth page
// turner presents itself, and MIDI messages) to reader actions. Bindings can be
// re-learned at runtime: pick an action, press the pedal.
class ActionMap : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList actions READ actions CONSTANT)
    Q_PROPERTY(QString learning READ learning NOTIFY learningChanged)
    Q_PROPERTY(int revision READ revision NOTIFY bindingsChanged)

public:
    explicit ActionMap(QObject *parent = nullptr);

    QVariantList actions() const;
    QString learning() const { return m_learning; }
    int revision() const { return m_revision; }

    Q_INVOKABLE bool handleKey(int key, int modifiers);
    Q_INVOKABLE QStringList bindingLabels(const QString &action) const;
    Q_INVOKABLE void learn(const QString &action);
    Q_INVOKABLE void cancelLearn();
    Q_INVOKABLE void clearBindings(const QString &action);
    Q_INVOKABLE void resetDefaults();

public slots:
    void handleMidi(int status, int data1, int data2);

signals:
    void triggered(const QString &action);
    void learningChanged();
    void bindingsChanged();

private:
    void dispatch(const QString &binding);
    void addBinding(const QString &action, const QString &binding);
    void load();
    void save() const;
    static QString label(const QString &binding);

    QHash<QString, QStringList> m_bindings; // action -> bindings
    QHash<int, int> m_ccState;              // last value per CC, to fire on press only
    QString m_learning;
    int m_revision = 0;
};
