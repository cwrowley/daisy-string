/*

  Created 19 Dec 2023
  Author: Clancy Rowley

  Copyright 2023 Clarence W. Rowley

*/

#include <math.h>
#include "daisy_pod.h"
#include "StiffString.h"


daisy::DaisyPod hw;
StiffString string;

const int NUM_MODES = 218;
const float NOTE_OFF_DECAY = 0.01;

volatile float _knob = 0.0f;
float amplitude = 0.0f;
float decay = 0.0f;
float decay_high_freq = 0.0f;
int current_note = 0;

inline float midi_to_freq(float m) {
  // Convert a MIDI note to frequency in Hz
  return powf(2, (m - 69.0f) / 12.0f) * 440.0f;
}

void AudioCallback(daisy::AudioHandle::InputBuffer in,
                   daisy::AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    float sample = amplitude * string.Tick();
    out[0][i] = sample;
    out[1][i] = sample;
  }
}

inline float MidiScale(int midi_value, float min, float max) {
  // Scale a MIDI parameter value (between 0 and 127) to the range [min, max]
  return min + static_cast<float>(midi_value) / 127.0f * (max - min);
}

void HandleMidiMessage(daisy::MidiEvent m) {
  switch (m.type) {
    case daisy::NoteOn: {
      auto p = m.AsNoteOn();
      current_note = p.note;
      string.set_freq(midi_to_freq(p.note));
      string.set_decay(decay);
      string.SetInitialAmplitudes();
      amplitude = MidiScale(p.velocity, 0.0f, 1.0f);
    }
      break;
    case daisy::NoteOff: {
      auto p = m.AsNoteOff();
      if (p.note == current_note) {
        string.set_decay(NOTE_OFF_DECAY);
      }
    }
      break;
    case daisy::ControlChange: {
      auto p = m.AsControlChange();
      switch (p.control_number) {
        case 1:
          string.set_stiffness(MidiScale(p.value, 0.0f, 0.2f));
          break;
        case 2:
          string.set_pluck_pos(MidiScale(p.value, 0.001f, 1.0f));
          break;
        case 3:
          decay_high_freq = MidiScale(p.value, 0.0f, 0.0005f);
          string.set_decay_high_freq(decay_high_freq);
          break;
        case 4:
          decay = MidiScale(p.value, 0.0f, 0.005f);
          string.set_decay(decay);
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
