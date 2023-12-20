/*

  Created 19 Dec 2023
  Author: Clancy Rowley

  Copyright 2023 Clarence W. Rowley

*/

#include <math.h>
#include "daisy_pod.h"
#include "StiffString.h"

const int NUM_MODES = 60;

daisy::DaisyPod hw;
StiffString string;

volatile float _knob = 0.f;

inline float midi_to_freq(float m) {
  // Convert a MIDI note to frequency in Hz
  return powf(2, (m - 69.0f) / 12.0f) * 440.0f;
}

void AudioCallback(daisy::AudioHandle::InputBuffer in,
                   daisy::AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    float sample = string.Tick();
    out[0][i] = sample;
    out[1][i] = sample;
  }
}

void HandleMidiMessage(daisy::MidiEvent m) {
  switch (m.type) {
    case daisy::NoteOn: {
      daisy::NoteOnEvent p = m.AsNoteOn();
      if (m.data[1] != 0) {
        p = m.AsNoteOn();
        string.set_freq(midi_to_freq(p.note));
        string.SetInitialAmplitudes();
        // string.set_amplitude(p.velocity / 127.0f);
      }
    }
      break;
    case daisy::NoteOff:
      break;
    case daisy::ControlChange: {
      daisy::ControlChangeEvent p = m.AsControlChange();
      switch (p.control_number) {
        case 1:
          string.set_stiffness(0.2f * (static_cast<float>(p.value / 127.0f)));
          break;
        default: break;
      }
    }
    default: break;
  }
}

int main(void) {
  hw.Init();
  hw.SetAudioBlockSize(4);  // number of samples handled per callback
  hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);
  hw.StartAdc();
  string.Init(hw.AudioSampleRate(), NUM_MODES);
  hw.StartAudio(AudioCallback);
  hw.midi.StartReceive();
  while (1) {
    hw.midi.Listen();
    while (hw.midi.HasEvents()) {
      HandleMidiMessage(hw.midi.PopEvent());
    }
    _knob = hw.GetKnobValue(hw.KNOB_1);
  }
}
