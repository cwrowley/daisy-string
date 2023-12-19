#include "daisy_pod.h"
#include "leaflet.h"
#include "StiffString.h"

using namespace daisy;

const float sample_rate = 48000.f;
const int NUM_MODES = 60;

DaisyPod hw;
StiffString string(sample_rate, NUM_MODES);

volatile float _knob = 0.f;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
  hw.ProcessAllControls();
  if (hw.button1.RisingEdge()) {
    string.SetInitialAmplitudes();
  }
  float knob2 = hw.GetKnobValue(hw.KNOB_2);
  string.set_stiffness(0.001f + knob2 * 0.1f);
  string.set_freq(_knob * 330.0f + 110.0f);
  for (size_t i = 0; i < size; i++) {
    float sample = string.Tick();
    out[0][i] = sample;
    out[1][i] = sample;
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
  while(1) {
    _knob = hw.GetKnobValue(hw.KNOB_1);
  }
}
