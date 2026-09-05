#include "AudioService.h"

#include <Wire.h>
#include <driver/i2s_std.h>

#include "pin_config.h"

namespace {
constexpr uint8_t kCodecAddress = 0x18;
constexpr uint32_t kSampleRate = 16000;
constexpr uint32_t kPublishIntervalMs = 40;
constexpr size_t kSamplesPerRead = 256;
constexpr size_t kFftSize = 256;
constexpr size_t kSpectrumBands = 32;
constexpr float kTwoPi = 6.28318530717958647692f;
// Hann coherent-gain and four-bin band-RMS compensation. This maps a
// full-scale bin-centred sine to approximately 1.0 without renormalizing each
// frame to whichever frequency happens to be strongest.
constexpr float kFftBandFullScale = 8.0f / kFftSize;
constexpr float kMinimumNoiseFloor = 0.0015f;
constexpr float kLoudRms = 0.18f;

float channelRms(const int16_t *samples, size_t sampleCount, size_t channel) {
  if (sampleCount < 2) return 0.0f;
  int64_t sum = 0;
  size_t count = 0;
  for (size_t index = channel; index < sampleCount; index += 2) {
    sum += samples[index];
    ++count;
  }
  if (!count) return 0.0f;
  const int32_t mean = static_cast<int32_t>(sum / static_cast<int64_t>(count));
  uint64_t sumSquares = 0;
  for (size_t index = channel; index < sampleCount; index += 2) {
    const int32_t centered = static_cast<int32_t>(samples[index]) - mean;
    sumSquares += static_cast<uint64_t>(static_cast<int64_t>(centered) * centered);
  }
  return sqrtf(static_cast<float>(sumSquares) / count) / 32768.0f;
}
}  // namespace

bool AudioService::writeCodecRegister(uint8_t address, uint8_t value) {
  Wire.beginTransmission(kCodecAddress);
  Wire.write(address);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool AudioService::readCodecRegister(uint8_t address, uint8_t &value) {
  Wire.beginTransmission(kCodecAddress);
  Wire.write(address);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(kCodecAddress, static_cast<uint8_t>(1)) != 1) return false;
  value = Wire.read();
  return true;
}

bool AudioService::configureCodec() {
  // Minimal 16 kHz, 16-bit analog-microphone setup from the ES8311 reference
  // sequence used by Waveshare's V2 Arduino example.
  if (!writeCodecRegister(0x00, 0x1f) ||
      !writeCodecRegister(0x00, 0x00) ||
      !writeCodecRegister(0x00, 0x80) ||
      !writeCodecRegister(0x01, 0x3f)) return false;

  uint8_t value = 0;
  if (!readCodecRegister(0x02, value) ||
      !writeCodecRegister(0x02, value & 0x07) ||
      !writeCodecRegister(0x03, 0x10) ||
      !writeCodecRegister(0x04, 0x10) ||
      !writeCodecRegister(0x05, 0x00)) return false;

  if (!readCodecRegister(0x06, value)) return false;
  value &= static_cast<uint8_t>(~0x20);
  if (!writeCodecRegister(0x06, value)) return false;
  if (!readCodecRegister(0x06, value)) return false;
  value = (value & 0xe0) | 0x03;
  if (!writeCodecRegister(0x06, value)) return false;

  if (!readCodecRegister(0x07, value) ||
      !writeCodecRegister(0x07, value & 0xc0) ||
      !writeCodecRegister(0x08, 0xff)) return false;

  if (!readCodecRegister(0x00, value) ||
      !writeCodecRegister(0x00, value & 0xbf) ||
      !writeCodecRegister(0x09, 0x0c) ||
      !writeCodecRegister(0x0a, 0x0c) ||
      !writeCodecRegister(0x0d, 0x01) ||
      !writeCodecRegister(0x0e, 0x02) ||
      !writeCodecRegister(0x12, 0x00) ||
      !writeCodecRegister(0x13, 0x10) ||
      !writeCodecRegister(0x1c, 0x6a) ||
      !writeCodecRegister(0x37, 0x08) ||
      !writeCodecRegister(0x17, 0xc8) ||
      !writeCodecRegister(0x14, 0x1a) ||
      !writeCodecRegister(0x16, 0x03)) return false;
  return true;
}

bool AudioService::begin() {
  i2s_.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
  if (!i2s_.begin(I2S_MODE_STD, kSampleRate, I2S_DATA_BIT_WIDTH_16BIT,
                  I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    return false;
  }
  ready_ = configureCodec();
  return ready_;
}

void AudioService::processSamples(const int16_t *samples, size_t sampleCount) {
  const float leftRms = channelRms(samples, sampleCount, 0);
  const float rightRms = channelRms(samples, sampleCount, 1);
  const float rms = max(leftRms, rightRms);
  if (!noiseFloorInitialized_) {
    noiseFloor_ = max(rms, kMinimumNoiseFloor);
    noiseFloorInitialized_ = true;
  } else if (rms < noiseFloor_ * 2.2f) {
    const float coefficient = rms < noiseFloor_ ? 0.025f : 0.0025f;
    noiseFloor_ += (max(rms, kMinimumNoiseFloor) - noiseFloor_) * coefficient;
  }

  const float gate = max(kMinimumNoiseFloor, noiseFloor_ * 1.65f);
  float target = constrain((rms - gate) / max(0.01f, kLoudRms - gate), 0.0f, 1.0f);
  target = sqrtf(target);
  const float smoothing = target > energy_ ? 0.38f : 0.055f;
  energy_ += (target - energy_) * smoothing;
  if (energy_ < 0.006f) energy_ = 0.0f;

  if (fftEnabled_) {
    if (fftSampleCount_ == 0) fftChannel_ = rightRms > leftRms ? 1 : 0;
    appendFftSamples(samples, sampleCount, fftChannel_);
  }
}

void AudioService::setFftEnabled(bool enabled) {
  if (enabled == fftEnabled_) return;
  fftEnabled_ = enabled;
  fftSampleCount_ = 0;
  spectrumReady_ = false;
}

bool AudioService::takeSpectrum(float output[32]) {
  if (!output || !spectrumReady_) return false;
  memcpy(output, spectrum_, sizeof(spectrum_));
  spectrumReady_ = false;
  return true;
}

void AudioService::appendFftSamples(const int16_t *samples, size_t sampleCount,
                                    size_t channel) {
  for (size_t index = channel; index < sampleCount; index += 2) {
    fftSamples_[fftSampleCount_++] = samples[index] / 32768.0f;
    if (fftSampleCount_ == kFftSize) {
      calculateSpectrum();
      fftSampleCount_ = 0;
    }
  }
}

void AudioService::calculateSpectrum() {
  float imaginary[kFftSize] = {};
  float mean = 0.0f;
  for (size_t index = 0; index < kFftSize; ++index) mean += fftSamples_[index];
  mean /= kFftSize;
  for (size_t index = 0; index < kFftSize; ++index) {
    const float window = 0.5f - 0.5f * cosf(kTwoPi * index / (kFftSize - 1));
    fftSamples_[index] = (fftSamples_[index] - mean) * window;
  }

  // In-place radix-2 FFT. A 256-sample transform is small enough to run well
  // inside the existing audio loop without a further library dependency.
  for (size_t index = 1, reversed = 0; index < kFftSize; ++index) {
    size_t bit = kFftSize >> 1;
    for (; reversed & bit; bit >>= 1) reversed ^= bit;
    reversed ^= bit;
    if (index < reversed) {
      const float real = fftSamples_[index];
      fftSamples_[index] = fftSamples_[reversed];
      fftSamples_[reversed] = real;
    }
  }
  for (size_t length = 2; length <= kFftSize; length <<= 1) {
    const float angle = -kTwoPi / length;
    const float stepReal = cosf(angle);
    const float stepImaginary = sinf(angle);
    for (size_t start = 0; start < kFftSize; start += length) {
      float twiddleReal = 1.0f;
      float twiddleImaginary = 0.0f;
      for (size_t offset = 0; offset < length / 2; ++offset) {
        const size_t even = start + offset;
        const size_t odd = even + length / 2;
        const float oddReal = fftSamples_[odd] * twiddleReal -
                              imaginary[odd] * twiddleImaginary;
        const float oddImaginary = fftSamples_[odd] * twiddleImaginary +
                                   imaginary[odd] * twiddleReal;
        const float evenReal = fftSamples_[even];
        const float evenImaginary = imaginary[even];
        fftSamples_[even] = evenReal + oddReal;
        imaginary[even] = evenImaginary + oddImaginary;
        fftSamples_[odd] = evenReal - oddReal;
        imaginary[odd] = evenImaginary - oddImaginary;
        const float nextReal = twiddleReal * stepReal -
                               twiddleImaginary * stepImaginary;
        twiddleImaginary = twiddleReal * stepImaginary +
                           twiddleImaginary * stepReal;
        twiddleReal = nextReal;
      }
    }
  }

  for (size_t band = 0; band < kSpectrumBands; ++band) {
    const size_t firstBin = 1 + band * 4;
    const size_t endBin = min(firstBin + 4, kFftSize / 2);
    float power = 0.0f;
    for (size_t bin = firstBin; bin < endBin; ++bin) {
      power += fftSamples_[bin] * fftSamples_[bin] +
               imaginary[bin] * imaginary[bin];
    }
    const float magnitude = endBin > firstBin
        ? sqrtf(power / (endBin - firstBin))
        : 0.0f;
    spectrum_[band] = constrain(magnitude * kFftBandFullScale, 0.0f, 1.0f);
  }
  spectrumReady_ = true;
}

bool AudioService::update() {
  if (!ready_) return false;
  int16_t samples[kSamplesPerRead];
  // A display transfer can take longer than one 8 ms audio block. Drain every
  // complete block already queued (within a generous bound) so FFT capture
  // always converges on the newest microphone samples instead of accumulating
  // latency behind the display renderer.
  for (uint8_t block = 0; block < 12; ++block) {
    size_t bytesRead = 0;
    const esp_err_t result = i2s_channel_read(i2s_.rxChan(), samples, sizeof(samples),
                                              &bytesRead, 0);
    if (result != ESP_OK || bytesRead < sizeof(int16_t) * 4) break;
    processSamples(samples, bytesRead / sizeof(int16_t));
  }

  const uint32_t now = millis();
  if (now - lastPublishMs_ < kPublishIntervalMs) return false;
  lastPublishMs_ = now;
  return true;
}
