/*
  ==============================================================================

    StiffString.cpp
    Created: 11 Dec 2023 7:55:35pm
    Author:  Clancy Rowley

  ==============================================================================
*/

#include "StiffString.h"
#include "math.h"
#include <new>

StiffString::StiffString(LEAF *leaf, int numModes)
: leaf_(leaf), num_modes_(numModes) {
  two_pi_times_inv_sample_rate_ = leaf->twoPiTimesInvSampleRate;
  tMempool *pool = leaf->mempool;
  amplitudes_ = (float *) mpool_alloc(numModes * sizeof(float), pool);
  output_weights_ = (float *) mpool_alloc(numModes * sizeof(float), pool);
  osc_ = (Cycle **) mpool_alloc(numModes * sizeof(Cycle *), pool);
  for (int i = 0; i < numModes; ++i) {
    osc_[i] = (Cycle *) mpool_alloc(sizeof(Cycle), pool);
    new (osc_[i]) Cycle(leaf->sampleRate);
  }
  UpdateOutputWeights();
}

StiffString::~StiffString() {
  tMempool *pool = leaf_->mempool;
  for (int i = 0; i < num_modes_; ++i) {
    osc_[i]->~Cycle();
    mpool_free(osc_[i], pool);
  }
  mpool_free(osc_, pool);
  mpool_free(output_weights_, pool);
  mpool_free(amplitudes_, pool);
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
        osc_[i]->set_freq(freq_hz_ * w);
    }
}

float StiffString::Tick() {
    float sample = 0.0f;
    for (int i = 0; i < num_modes_; ++i) {
      sample += osc_[i]->Tick() * amplitudes_[i] * output_weights_[i];
      int n = i + 1;
      float sig = decay_ + decay_high_freq_ * n * n;
      //amplitudes[i] *= expf(-sig * freqHz * leaf->twoPiTimesInvSampleRate);
      sig = LEAF_clip(0.f, sig, 1.f);
      amplitudes_[i] *= 1.0f -sig * freq_hz_ * two_pi_times_inv_sample_rate_;
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
