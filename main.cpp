#include "daisy_pod.h"
#include "daisysp.h"
#include "leaflet.h"
#include "StiffString.h"

using namespace daisy;

const float sample_rate = 48000.f;
const int NUM_MODES = 60;

DaisyPod hw;
StiffString string(sample_rate, NUM_MODES);

volatile float _knob = 0.f;

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size) {
  for (size_t i = 0; i < size; i++) {
    float sample = string.Tick();
    out[0][i] = sample;
    out[1][i] = sample;
  }
}

void HandleMidiMessage(MidiEvent m) {
  switch (m.type) {
  case NoteOn:
    {
      NoteOnEvent p = m.AsNoteOn();
      if (m.data[1] != 0) {
        p = m.AsNoteOn();
        string.set_freq(daisysp::mtof(p.note));
        string.SetInitialAmplitudes();
        // string.set_amplitude(p.velocity / 127.0f);
      }
    }
    break;
  case NoteOff:
    break;
  case ControlChange:
    {
      ControlChangeEvent p = m.AsControlChange();
      switch(p.control_number) {
      case 1:
        string.set_stiffness(0.2f * ((float) p.value / 127.0f));
        break;
      default: break;
      }
    }
  default: break;
  }

}

int main(void) {
  hw.Init();
  hw.SetAudioBlockSize(4); // number of samples handled per callback
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
  hw.StartAdc();
  float sample_rate = 48000.f;
  string.Init(sample_rate, NUM_MODES);
  hw.StartAudio(AudioCallback);
  hw.midi.StartReceive();
  while(1) {
    hw.midi.Listen();
    while (hw.midi.HasEvents()) {
      HandleMidiMessage(hw.midi.PopEvent());
    }
    _knob = hw.GetKnobValue(hw.KNOB_1);
  }
}
