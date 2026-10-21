// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>

#include <memory>
#include <vector>

#ifdef OPENSCORE_WITH_MIDI
class RtMidiIn;
#endif

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
    QStringList m_ports;
#ifdef OPENSCORE_WITH_MIDI
    std::vector<std::unique_ptr<RtMidiIn>> m_inputs;
#endif
};
