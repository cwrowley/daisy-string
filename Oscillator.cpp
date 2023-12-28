/*
  Oscillator.cpp
  Clancy Rowley

  Copyright 2023 Clarence W. Rowley
*/

#include "Oscillator.h"
#include <math.h>

const float TWO_PI = 8.0f * atanf(1.0f);

Oscillator::Oscillator(float sample_rate) {
  freq_ = 0.0f;
  set_sample_rate(sample_rate);
}

Oscillator::~Oscillator() {}

void Oscillator::set_freq(float freq_hz) {
  bool first_time = (freq_ == 0);
  freq_ = freq_hz;
  loop_gain_ = cosf(freq_hz * two_pi_by_sample_rate_);
  float g = sqrt((1 - loop_gain_) / (1 + loop_gain_));
  if (first_time) {
    turns_ratio_ = g;
    ResetPhase();
    return;
  }
  // scale state variable in preparation for the next step
  x_ *= g / turns_ratio_;
  turns_ratio_ = g;
}

void Oscillator::ResetPhase() {
  x_ = turns_ratio_;
  y_ = 0.0f;
}

void Oscillator::set_sample_rate(float sr) {
  two_pi_by_sample_rate_ = TWO_PI / sr;
}
