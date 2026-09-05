#pragma once

#include <Arduino.h>
#include <ESP_I2S.h>

class AudioService {
 public:
  bool begin();
  bool update();
  bool ready() const { return ready_; }
  float energy() const { return energy_; }
  void setFftEnabled(bool enabled);
  bool takeSpectrum(float output[32]);

 private:
  bool configureCodec();
  bool writeCodecRegister(uint8_t address, uint8_t value);
  bool readCodecRegister(uint8_t address, uint8_t &value);
  void processSamples(const int16_t *samples, size_t sampleCount);
  void appendFftSamples(const int16_t *samples, size_t sampleCount, size_t channel);
  void calculateSpectrum();

  I2SClass i2s_;
  bool ready_ = false;
  bool noiseFloorInitialized_ = false;
  float noiseFloor_ = 0.004f;
  float energy_ = 0.0f;
  uint32_t lastPublishMs_ = 0;
  bool fftEnabled_ = false;
  bool spectrumReady_ = false;
  uint16_t fftSampleCount_ = 0;
  uint8_t fftChannel_ = 0;
  float fftSamples_[256] = {};
  float spectrum_[32] = {};
};
