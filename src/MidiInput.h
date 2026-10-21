// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>

#include <memory>

// Listens on every MIDI input port (USB / Bluetooth MIDI pedals, keyboards,
// controllers) and forwards raw messages. Compiles to a no-op without RtMidi.
class MidiInput : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QStringList ports READ ports NOTIFY portsChanged)

public:
    explicit MidiInput(QObject *parent = nullptr);
    ~MidiInput() override;

    bool available() const;
    QStringList ports() const { return m_ports; }

    Q_INVOKABLE void refresh();

signals:
    void message(int status, int data1, int data2);
    void portsChanged();

private:
    // RtMidi 6 moved its classes into a namespace, so forward-declaring
    // RtMidiIn here would clash. Keep RtMidi out of the header entirely.
    struct Impl;
    std::unique_ptr<Impl> d;
    QStringList m_ports;
};
