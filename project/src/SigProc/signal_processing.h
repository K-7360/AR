/**
 * @file signal_processing.h
 * @brief EEG Signal Processing API
 *
 * Processes raw EEG signals (250Hz) through filtering and FFT
 * to generate cognitive indices (Focus, Relaxation, Fatigue, Meditation)
 */

#ifndef SIGNAL_PROCESSING_H
#define SIGNAL_PROCESSING_H

#include <stdint.h>

#define SAMPLE_RATE 250
#define FFT_SIZE 1024
#define FFT_UPDATE_SAMPLES 256  // Update FFT every 256 samples (~1 Hz)
#define DC_WINDOW_SIZE 250      // 1 second window for DC removal
#define SMOOTH_WINDOW_SIZE 5    // Moving average window

// Band power structure
typedef struct {
    float delta;   // 0.5-4 Hz
    float theta;   // 4-8 Hz
    float alpha;   // 8-13 Hz
    float beta;    // 13-30 Hz
    float gamma;   // 30-50 Hz
    float total;   // Total power
} band_power_t;

// Cognitive indices structure
typedef struct {
    float focus;       // 0-100
    float relaxation;  // 0-100
    float fatigue;     // 0-100
    float meditation;  // 0-100
} signal_indices_t;

/**
 * @brief Initialize all signal processing components
 * Must be called before any other signal processing functions
 */
void signal_init(void);

/**
 * @brief Process a single raw sample through the filter chain
 * @param raw_sample Raw ADC sample from EEG
 * @param channel Channel number (0 or 1)
 * @return Filtered and smoothed sample ready for display
 */
float signal_process(int32_t raw_sample, uint8_t channel);

/**
 * @brief Check if sample was marked as artifact
 * @param channel Channel number (0 or 1)
 * @return 1 if artifact detected, 0 otherwise
 */
uint8_t signal_is_artifact(uint8_t channel);

/**
 * @brief Get current band powers for a channel
 * @param channel Channel number (0 or 1)
 * @param power Pointer to store band power values
 */
void signal_get_band_power(uint8_t channel, band_power_t *power);

/**
 * @brief Get current cognitive indices (averaged from both channels)
 * @param indices Pointer to store cognitive indices
 */
void signal_get_indices(signal_indices_t *indices);

/**
 * @brief Check if new indices are available (after FFT update)
 * @return 1 if new indices available, 0 otherwise
 */
uint8_t signal_indices_updated(void);

/**
 * @brief Set notch filter frequency (50 or 60 Hz)
 * @param freq Frequency in Hz (50 or 60)
 */
void signal_set_notch_freq(float freq);

/**
 * @brief Set smoothing factor for display waveform
 * @param alpha EMA alpha value (0-1, higher = less smoothing)
 */
void signal_set_smoothing(float alpha);

#endif // SIGNAL_PROCESSING_H
