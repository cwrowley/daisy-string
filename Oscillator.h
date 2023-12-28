/*
  Oscillator.h
  Clancy Rowley

  Digital waveguide oscillator, as described in Smith and Cook (1992)

  The version implemented here is without any damping/decay.

  Reference:
  J. O. Smith and P. R. Cook. The second-order digital waveguide oscillator.
  In Proceedings of the International Computer Music Conference, pages
  150-153, Oct. 1992.

  Copyright 2023 Clarence W. Rowley
*/

#pragma once

class Oscillator {
 public:
  Oscillator() : Oscillator(1.0f) {}
  explicit Oscillator(float sample_rate);
  ~Oscillator();

  inline float Tick();
  void ResetPhase();

  // change parameters
  void set_freq(float freq);
  void set_sample_rate(float sr);

 private:
  float freq_;
  float two_pi_by_sample_rate_;
  float loop_gain_;
  float turns_ratio_;

  // state variables
  float x_;
  float y_;
};

inline float Oscillator::Tick() {
  float w = x_;
  float z = loop_gain_ * (y_ + w);
  x_ = z - y_;
  y_ = z + w;
  return y_;
}
