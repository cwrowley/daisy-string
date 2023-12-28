/*
  ==============================================================================

    StiffString.h
    Created: 11 Dec 2023 7:55:35pm
    Author:  Clancy Rowley

    Copyright 2023 Clarence W. Rowley

  ==============================================================================
*/

#pragma once

#include "Oscillator.h"

const int MAX_NUM_MODES = 400;

class StiffString {
 public:
  StiffString();
  StiffString(float sample_rate, int num_modes);
  ~StiffString();

  void Init(float sample_rate, int num_modes);
  void SetInitialAmplitudes();
  float Tick();

  // change parameters
  void set_sample_rate(float sr);
  void set_freq(float newFreqHz);
  void set_stiffness(float newValue) { stiffness_ = newValue; }
  void set_pickup_pos(float newValue);
  void set_pluck_pos(float newValue) { pluck_pos_ = newValue; }
  void set_decay(float newValue);
  void set_decay_high_freq(float newValue);

 private:
  void UpdateOscillators();
  void UpdateOutputWeights();

  int num_modes_;
  float sample_rate_;
  float two_pi_by_sample_rate_;

  Oscillator osc_[MAX_NUM_MODES];
  float amplitudes_[MAX_NUM_MODES];
  float decay_rates_[MAX_NUM_MODES];
  float output_weights_[MAX_NUM_MODES];
  float freq_hz_;

  // parameters
  float stiffness_ = 0.001f;
  float pluck_pos_ = 0.2f;
  float pickup_pos_ = 0.3f;
  float decay_ = 0.0001f;
  float decay_high_freq_ = 0.0003f;
};
