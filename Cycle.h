/*
  Cycle.h
  Clancy Rowley

  Copyright 2023 Clarence W. Rowley
*/

#pragma once

#include <stdlib.h>

const int SINE_TABLE_BITS = 11;  // 2048-long table

extern const float __sine_table[1 << SINE_TABLE_BITS];

class Cycle {
 public:
  Cycle() {}
  explicit Cycle(float sample_rate);
  ~Cycle();

  inline float Tick() {
    // Phasor increment
    phase_ += inc_;
    // Wavetable synthesis
    const char frac_bits = 32 - SINE_TABLE_BITS;
    const uint32_t frac_mask = (1 << frac_bits) - 1;
    uint32_t idx = phase_ >> frac_bits;
    uint32_t frac = phase_ & frac_mask;
    const float one_over_delta_sample = 1.0f / (1 << frac_bits);
    float delta = static_cast<float>(frac * one_over_delta_sample);

    const uint32_t table_mask = (1 << SINE_TABLE_BITS) - 1;
    float samp0 = __sine_table[idx];
    idx = (idx + 1) & table_mask;
    float samp1 = __sine_table[idx];

    return samp0 + (samp1 - samp0) * delta;
  }

  // change parameters
  void set_freq(float freq);
  void set_phase(float phase);
  void set_sample_rate(float sr);

 private:
  // Underlying phasor
  uint32_t phase_;
  int32_t inc_;
  float freq_;
  float sample_rate_;
  float inv_sample_rate_times_two_to_32_;
};
