#pragma once

#include <stdlib.h>

class Cycle {
public:
  Cycle(float sample_rate);
  ~Cycle();

  float Tick();

  // change parameters
  void set_freq(float freq);
  void set_phase(float phase);
  void set_sample_rate(float sr);

private:
  float sample_rate_;
  // Underlying phasor
  uint32_t phase_;
  int32_t inc_;
  float freq_;
  float inv_sample_rate_times_two_to_32_;
};
