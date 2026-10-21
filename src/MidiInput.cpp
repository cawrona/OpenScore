// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MidiInput.h"

#include <QMetaObject>

#ifdef OPENSCORE_WITH_MIDI
#include <RtMidi.h>

namespace {
void onMidi(double, std::vector<unsigned char> *msg, void *user)
{
    if (!msg || msg->size() < 2)
        return;
    auto *self = static_cast<MidiInput *>(user);
    const int st = (*msg)[0];
    const int d1 = (*msg)[1];
    const int d2 = msg->size() > 2 ? (*msg)[2] : 0;
    // RtMidi calls this from its own thread; hop to the GUI thread.
    QMetaObject::invokeMethod(
        self, [self, st, d1, d2] { emit self->message(st, d1, d2); }, Qt::QueuedConnection);
}
} // namespace
#endif

MidiInput::MidiInput(QObject *parent)
    : QObject(parent)
{
    refresh();
}

MidiInput::~MidiInput() = default;

bool MidiInput::available() const
{
#ifdef OPENSCORE_WITH_MIDI
    return true;
#else
    return false;
#endif
}

void MidiInput::refresh()
{
#ifdef OPENSCORE_WITH_MIDI
    m_inputs.clear();
    m_ports.clear();
    try {
        RtMidiIn probe;
        const unsigned n = probe.getPortCount();
        for (unsigned i = 0; i < n; ++i) {
            auto in = std::make_unique<RtMidiIn>();
            in->openPort(i);
            in->ignoreTypes(true, true, true); // sysex, timing, active sensing
            in->setCallback(&onMidi, this);
            m_ports << QString::fromStdString(probe.getPortName(i));
            m_inputs.push_back(std::move(in));
        }
    } catch (RtMidiError &e) {
        qWarning("MIDI: %s", e.getMessage().c_str());
    }
    emit portsChanged();
#endif
}
