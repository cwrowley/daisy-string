/*
  ==============================================================================

    StiffString.cpp
    Created: 11 Dec 2023 7:55:35pm
    Author:  Clancy Rowley

  ==============================================================================
*/

#include "StiffString.h"
#include "math.h"
#include <cassert>

const float PI = 4.0f * atanf(1.0f);
const float TWO_PI = 2.0f * PI;

StiffString::StiffString()
: num_modes_(0), sample_rate_(0.f) {}

StiffString::StiffString(float sample_rate, int num_modes)
: num_modes_(num_modes) {
  assert(num_modes <= MAX_NUM_MODES);
  set_sample_rate(sample_rate);
  UpdateOutputWeights();
}

StiffString::~StiffString() {}

void StiffString::Init(float sample_rate, int num_modes) {
  num_modes_ = num_modes;
  assert(num_modes <= MAX_NUM_MODES);
  set_sample_rate(sample_rate);
}

void StiffString::set_sample_rate(float sample_rate) {
  sample_rate_ = sample_rate;
  two_pi_by_sample_rate_ = TWO_PI / sample_rate;
  assert(num_modes_ > 0 && num_modes_ <= MAX_NUM_MODES);
  for (int i = 0; i < num_modes_; ++i) {
    osc_[i].set_sample_rate(sample_rate);
  }

}

void StiffString::set_freq(float freq_hz) {
    freq_hz_ = freq_hz;
    float kappa_sq = stiffness_ * stiffness_;
    for (int i = 0; i < num_modes_; ++i) {
        int n = i + 1;
        int n_sq = n * n;
        float sig = decay_ + decay_high_freq_ * n_sq;
        float w0 = n * sqrtf(1.0f + kappa_sq * n_sq);
        // float w0 = n * (1.0f + 0.5f * kappa_sq * n_sq);
        float zeta = sig / w0;
        float w = w0 * sqrtf(1.0f - zeta * zeta);
        // float w = w0 * (1.0f - 0.5f * zeta * zeta);
        osc_[i].set_freq(freq_hz_ * w);
    }
}

inline float clip(float val, float min = 0.0f, float max = 1.0f) {
  if (val < min) {
    return min;
  } else if (val < max) {
    return val;
  }
  return max;
}

float StiffString::Tick() {
    float sample = 0.0f;
    for (int i = 0; i < num_modes_; ++i) {
      sample += osc_[i].Tick() * amplitudes_[i] * output_weights_[i];
      int n = i + 1;
      float sig = decay_ + decay_high_freq_ * n * n;
      //amplitudes[i] *= expf(-sig * freqHz * leaf->twoPiTimesInvSampleRate);
      sig = clip(sig, 0.0f, 1.0f);
      amplitudes_[i] *= 1.0f -sig * freq_hz_ * two_pi_by_sample_rate_;
    }
    return sample;
}

void StiffString::set_pickup_pos(float newValue) {
    pickup_pos_ = newValue;
    UpdateOutputWeights();
}

void StiffString::UpdateOutputWeights() {
    float x0 = pickup_pos_ * 0.5 * PI;
    for (int i = 0; i < num_modes_; ++i) {
        output_weights_[i] = sinf((i + 1) * x0);
    }
}

void StiffString::SetInitialAmplitudes() {
    float x0 = pluck_pos_ * 0.5 * PI;
    for (int i = 0; i < num_modes_; ++i) {
        int n = i + 1;
        float denom = n * n * x0 * (PI - x0);
        amplitudes_[i] = 2.0f * sinf(x0 * n) / denom;
    }
}
