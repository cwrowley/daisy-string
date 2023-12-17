#include "Cycle.h"
#include <math.h>

const float TWO_TO_32 = 4294967296.0f;

Cycle::Cycle(float sample_rate)
: sample_rate_(sample_rate) {
  phase_ = 0;
  inc_ = 0;
  freq_ = 0.f;
  inv_sample_rate_times_two_to_32_ = TWO_TO_32 / sample_rate;
}

Cycle::~Cycle() {}

void Cycle::set_freq(float freq_hz) {
  freq_ = freq_hz;
  inc_ = freq_hz * inv_sample_rate_times_two_to_32_;
}

void Cycle::set_phase(float phase) {
  phase -= (int) phase;
  phase_ = phase * TWO_TO_32;
}
