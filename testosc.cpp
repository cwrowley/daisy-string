#include <stdio.h>
#include "Oscillator.h"

int main() {
  const int sr = 1000;
  Oscillator osc(static_cast<float>(sr));
  float base_freq = 100.0f;
  osc.set_freq(base_freq);

  const int num_seconds = 1000;
  const int num_samples = sr * num_seconds;
  for (int i = 0; i < num_seconds; ++i) {
    for (int j = 0; j < sr; ++j) {
      float sample = osc.Tick();
      printf("%f\n", sample);
    }
    osc.set_freq(base_freq * ((i % 2) + 1));
  }
  return 0;
}
