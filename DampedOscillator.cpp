/*
  DampedOscillator.cpp
  Clancy Rowley

  Copyright 2023 Clarence W. Rowley
*/

#include "DampedOscillator.h"
#include <math.h>

const float TWO_PI = 8.0f * atanf(1.0f);

DampedOscillator::DampedOscillator(float sample_rate) {
  freq_ = 0.0f;
  decay_ = 1.0f;
  set_sample_rate(sample_rate);
}

DampedOscillator::~DampedOscillator() {}

void DampedOscillator::set_freq(float freq_hz) {
  bool first_time = (freq_ == 0);
  freq_ = freq_hz;
  loop_gain_ = cosf(freq_hz * two_pi_by_sample_rate_);
  float g = sqrt((1 - loop_gain_) / (1 + loop_gain_));
  if (first_time) {
    turns_ratio_ = g;
    Reset();
    return;
  }
  // scale state variable in preparation for the next step
  x_ *= g / turns_ratio_;
  turns_ratio_ = g;
}

void DampedOscillator::set_decay(float decay) {
  float r = exp(-decay * two_pi_by_sample_rate_);
  decay_ = r * r;
}

void DampedOscillator::Reset() {
  x_ = turns_ratio_;
  y_ = 0.0f;
}

void DampedOscillator::set_sample_rate(float sr) {
  two_pi_by_sample_rate_ = TWO_PI / sr;
}
