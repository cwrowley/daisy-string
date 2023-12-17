/*
  ==============================================================================

    StiffString.h
    Created: 11 Dec 2023 7:55:35pm
    Author:  Clancy Rowley

  ==============================================================================
*/

#pragma once

#include "leaflet.h"
#include "Cycle.h"

const int MAX_NUM_MODES = 100;

class StiffString {
public:
  StiffString(LEAF *leaf, int numModes);
  ~StiffString();

  void SetInitialAmplitudes();
  float Tick();

  // change parameters
  void set_freq(float newFreqHz);
  void set_stiffness(float newValue) { stiffness_ = newValue; }
  void set_pickup_pos(float newValue);
  void set_pluck_pos(float newValue) { pluck_pos_ = newValue; }
  void set_decay(float newValue) { decay_ = newValue; }
  void set_decay_high_freq(float newValue) { decay_high_freq_ = newValue; }

private:
  void UpdateOutputWeights();

  LEAF *const leaf_;
  const int num_modes_;

  Cycle osc_[MAX_NUM_MODES];
  float amplitudes_[MAX_NUM_MODES];
  float output_weights_[MAX_NUM_MODES];
  float freq_hz_;
  float two_pi_times_inv_sample_rate_;

  // parameters
  float stiffness_ = 0.001f;
  float pluck_pos_ = 0.2f;
  float pickup_pos_ = 0.3f;
  float decay_ = 0.0001f;
  float decay_high_freq_ = 0.0003f;
};
