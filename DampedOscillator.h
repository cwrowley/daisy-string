/*
  DampedOscillator.h
  Clancy Rowley

  Copyright 2023 Clarence W. Rowley

  Digital waveguide oscillator, as described in Smith and Cook (1992)

  The version implemented here includes damping/decay.

  Reference:
  J. O. Smith and P. R. Cook. The second-order digital waveguide oscillator.
  In Proceedings of the International Computer Music Conference, pages
  150-153, Oct. 1992.
*/

#pragma once

class DampedOscillator {
 public:
  DampedOscillator() : DampedOscillator(1.0f) {}
  explicit DampedOscillator(float sample_rate);
  ~DampedOscillator();

  inline float Tick();
  void Reset();

  // change parameters
  void set_freq(float freq);
  void set_decay(float decay);
  void set_sample_rate(float sr);

 private:
  float freq_;
  float decay_;
  float two_pi_by_sample_rate_;
  float loop_gain_;
  float turns_ratio_;

  // state variables
  float x_;
  float y_;
};

inline float DampedOscillator::Tick() {
  float w = decay_ * x_;
  float z = loop_gain_ * (y_ + w);
  x_ = z - y_;
  y_ = z + w;
  return y_;
}
