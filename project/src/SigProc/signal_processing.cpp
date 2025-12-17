/**
 * @file signal_processing.cpp
 * @brief EEG Signal Processing Implementation
 *
 * All issues from Phase2.md are FIXED:
 * - DC removal: uses float sum (no overflow)
 * - Moving avg: uses count (no div-by-zero), buffer initialized
 * - FFT: uses KissFFT library
 * - SMR: approximated as 30% of beta power
 * - math.h: included
 */

#include "signal_processing.h"
#include <math.h>
#include <string.h>

// KissFFT includes
extern "C" {
#include "kissfft/kiss_fftr.h"
}

// ============================================================================
// DC Removal (FIXED: use float sum to avoid overflow)
// ============================================================================
typedef struct {
    float buffer[DC_WINDOW_SIZE];
    float sum;          // FIXED: float instead of int32_t
    uint16_t index;
    uint16_t count;
} dc_removal_t;

static void dc_removal_init(dc_removal_t *state) {
    memset(state->buffer, 0, sizeof(state->buffer));
    state->sum = 0;
    state->index = 0;
    state->count = 0;
}

static float dc_remove(dc_removal_t *state, float sample) {
    // Subtract oldest value from sum
    if (state->count >= DC_WINDOW_SIZE) {
        state->sum -= state->buffer[state->index];
    }

    // Add new value
    state->buffer[state->index] = sample;
    state->sum += sample;

    // Update index
    state->index = (state->index + 1) % DC_WINDOW_SIZE;
    if (state->count < DC_WINDOW_SIZE) state->count++;

    // Return sample minus mean
    float mean = state->sum / state->count;
    return sample - mean;
}

// ============================================================================
// IIR Notch Filter (50/60 Hz)
// ============================================================================
typedef struct {
    float b0, b1, b2;  // Numerator coefficients
    float a1, a2;      // Denominator coefficients
    float x1, x2;      // Input history
    float y1, y2;      // Output history
} notch_filter_t;

static void notch_init_50hz(notch_filter_t *f) {
    // Pre-calculated coefficients for 50 Hz notch at 250 Hz, Q=30
    f->b0 = 0.9695f;
    f->b1 = -1.2339f;
    f->b2 = 0.9695f;
    f->a1 = -1.2339f;
    f->a2 = 0.9391f;
    f->x1 = f->x2 = f->y1 = f->y2 = 0;
}

static void notch_init_60hz(notch_filter_t *f) {
    // Pre-calculated coefficients for 60 Hz notch at 250 Hz, Q=30
    f->b0 = 0.9695f;
    f->b1 = -0.9391f;
    f->b2 = 0.9695f;
    f->a1 = -0.9391f;
    f->a2 = 0.9391f;
    f->x1 = f->x2 = f->y1 = f->y2 = 0;
}

static float notch_filter(notch_filter_t *f, float x) {
    float y = f->b0 * x + f->b1 * f->x1 + f->b2 * f->x2
              - f->a1 * f->y1 - f->a2 * f->y2;

    f->x2 = f->x1;
    f->x1 = x;
    f->y2 = f->y1;
    f->y1 = y;

    return y;
}

// ============================================================================
// Bandpass Filter (0.5-50 Hz) - Cascaded IIR
// ============================================================================
typedef struct {
    // High-pass section (0.5 Hz cutoff at 250 Hz)
    float hp_b0, hp_b1, hp_b2;
    float hp_a1, hp_a2;
    float hp_x1, hp_x2, hp_y1, hp_y2;

    // Low-pass section (50 Hz cutoff at 250 Hz)
    float lp_b0, lp_b1, lp_b2;
    float lp_a1, lp_a2;
    float lp_x1, lp_x2, lp_y1, lp_y2;
} bandpass_filter_t;

static void bandpass_init(bandpass_filter_t *f) {
    // High-pass 0.5 Hz at 250 Hz (2nd order Butterworth)
    f->hp_b0 = 0.9911f;
    f->hp_b1 = -1.9822f;
    f->hp_b2 = 0.9911f;
    f->hp_a1 = -1.9822f;
    f->hp_a2 = 0.9823f;
    f->hp_x1 = f->hp_x2 = f->hp_y1 = f->hp_y2 = 0;

    // Low-pass 50 Hz at 250 Hz (2nd order Butterworth)
    f->lp_b0 = 0.2929f;
    f->lp_b1 = 0.5858f;
    f->lp_b2 = 0.2929f;
    f->lp_a1 = -0.0f;
    f->lp_a2 = 0.1716f;
    f->lp_x1 = f->lp_x2 = f->lp_y1 = f->lp_y2 = 0;
}

static float bandpass_filter(bandpass_filter_t *f, float x) {
    // High-pass section
    float hp_y = f->hp_b0 * x + f->hp_b1 * f->hp_x1 + f->hp_b2 * f->hp_x2
                 - f->hp_a1 * f->hp_y1 - f->hp_a2 * f->hp_y2;
    f->hp_x2 = f->hp_x1;
    f->hp_x1 = x;
    f->hp_y2 = f->hp_y1;
    f->hp_y1 = hp_y;

    // Low-pass section
    float lp_y = f->lp_b0 * hp_y + f->lp_b1 * f->lp_x1 + f->lp_b2 * f->lp_x2
                 - f->lp_a1 * f->lp_y1 - f->lp_a2 * f->lp_y2;
    f->lp_x2 = f->lp_x1;
    f->lp_x1 = hp_y;
    f->lp_y2 = f->lp_y1;
    f->lp_y1 = lp_y;

    return lp_y;
}

// ============================================================================
// Artifact Detection
// ============================================================================
typedef struct {
    float amplitude_threshold;  // uV
    float gradient_threshold;   // uV/sample
    float last_sample;
    uint8_t artifact_detected;
} artifact_detector_t;

static void artifact_init(artifact_detector_t *d) {
    d->amplitude_threshold = 100.0f;  // 100 uV
    d->gradient_threshold = 50.0f;    // 50 uV/sample
    d->last_sample = 0;
    d->artifact_detected = 0;
}

static uint8_t detect_artifact(artifact_detector_t *d, float sample) {
    // Amplitude check
    if (fabsf(sample) > d->amplitude_threshold) {
        d->artifact_detected = 1;
        d->last_sample = sample;
        return 1;
    }

    // Gradient check
    float gradient = fabsf(sample - d->last_sample);
    if (gradient > d->gradient_threshold) {
        d->artifact_detected = 1;
        d->last_sample = sample;
        return 1;
    }

    d->last_sample = sample;
    d->artifact_detected = 0;
    return 0;
}

// ============================================================================
// Moving Average Filter (FIXED: handle first call properly)
// ============================================================================
typedef struct {
    float buffer[SMOOTH_WINDOW_SIZE];
    float sum;
    uint8_t index;
    uint8_t count;  // FIXED: use count instead of filled flag
} moving_avg_t;

static void moving_avg_init(moving_avg_t *state) {
    memset(state->buffer, 0, sizeof(state->buffer));  // FIXED: clear buffer
    state->sum = 0;
    state->index = 0;
    state->count = 0;
}

static float moving_avg(moving_avg_t *state, float sample) {
    // FIXED: Remove oldest only if buffer is full
    if (state->count >= SMOOTH_WINDOW_SIZE) {
        state->sum -= state->buffer[state->index];
    }

    // Add new
    state->buffer[state->index] = sample;
    state->sum += sample;

    // Update index and count
    state->index = (state->index + 1) % SMOOTH_WINDOW_SIZE;
    if (state->count < SMOOTH_WINDOW_SIZE) state->count++;

    // FIXED: count is always >= 1 after adding sample, no div-by-zero
    return state->sum / state->count;
}

// ============================================================================
// Exponential Moving Average (EMA)
// ============================================================================
typedef struct {
    float alpha;
    float ema;
    uint8_t initialized;
} ema_filter_t;

static void ema_init(ema_filter_t *f, float alpha) {
    f->alpha = alpha;
    f->ema = 0;
    f->initialized = 0;
}

static float ema_filter(ema_filter_t *f, float sample) {
    if (!f->initialized) {
        f->ema = sample;
        f->initialized = 1;
        return sample;
    }
    f->ema = f->alpha * sample + (1.0f - f->alpha) * f->ema;
    return f->ema;
}

// ============================================================================
// FFT Processor (using KissFFT)
// ============================================================================
typedef struct {
    float input_buffer[FFT_SIZE];
    kiss_fft_cpx freq_output[FFT_SIZE / 2 + 1];
    float magnitude[FFT_SIZE / 2 + 1];
    uint16_t buffer_index;
    uint16_t sample_count;
    kiss_fftr_cfg cfg;
} fft_processor_t;

static void fft_init(fft_processor_t *p) {
    memset(p->input_buffer, 0, sizeof(p->input_buffer));
    memset(p->magnitude, 0, sizeof(p->magnitude));
    p->buffer_index = 0;
    p->sample_count = 0;
    p->cfg = kiss_fftr_alloc(FFT_SIZE, 0, NULL, NULL);
}

static void fft_add_sample(fft_processor_t *p, float sample) {
    p->input_buffer[p->buffer_index] = sample;
    p->buffer_index = (p->buffer_index + 1) % FFT_SIZE;
    p->sample_count++;
}

static uint8_t fft_ready(fft_processor_t *p) {
    if (p->sample_count >= FFT_UPDATE_SAMPLES) {
        p->sample_count = 0;
        return 1;
    }
    return 0;
}

static void fft_compute(fft_processor_t *p) {
    // Reorder buffer to start from oldest sample
    float temp[FFT_SIZE];
    for (int i = 0; i < FFT_SIZE; i++) {
        temp[i] = p->input_buffer[(p->buffer_index + i) % FFT_SIZE];
    }

    // Apply Hanning window
    for (int i = 0; i < FFT_SIZE; i++) {
        float window = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (FFT_SIZE - 1)));
        temp[i] *= window;
    }

    // Perform FFT using KissFFT
    kiss_fftr(p->cfg, temp, p->freq_output);

    // Calculate magnitude
    for (int i = 0; i < FFT_SIZE / 2 + 1; i++) {
        float re = p->freq_output[i].r;
        float im = p->freq_output[i].i;
        p->magnitude[i] = sqrtf(re * re + im * im);
    }
}

// ============================================================================
// Band Power Calculation
// ============================================================================
static void calculate_band_power(fft_processor_t *fft, band_power_t *power) {
    float bin_width = (float)SAMPLE_RATE / FFT_SIZE;

    power->delta = 0;
    power->theta = 0;
    power->alpha = 0;
    power->beta = 0;
    power->gamma = 0;

    for (int i = 1; i < FFT_SIZE / 2; i++) {
        float freq = i * bin_width;
        float mag_squared = fft->magnitude[i] * fft->magnitude[i];

        if (freq >= 0.5f && freq < 4.0f) {
            power->delta += mag_squared;
        } else if (freq >= 4.0f && freq < 8.0f) {
            power->theta += mag_squared;
        } else if (freq >= 8.0f && freq < 13.0f) {
            power->alpha += mag_squared;
        } else if (freq >= 13.0f && freq < 30.0f) {
            power->beta += mag_squared;
        } else if (freq >= 30.0f && freq < 50.0f) {
            power->gamma += mag_squared;
        }
    }

    power->total = power->delta + power->theta + power->alpha +
                   power->beta + power->gamma;
}

// ============================================================================
// Cognitive Index Calculations
// ============================================================================
static float calculate_focus_index(band_power_t *power) {
    if (power->theta < 0.001f) power->theta = 0.001f;

    // Beta/Theta ratio
    float beta_theta_ratio = power->beta / power->theta;

    // FIXED: SMR (12-15 Hz) approximated as 30% of beta power
    float smr_contribution = power->beta * 0.3f / (power->total + 0.001f) * 50.0f;

    // Combine metrics
    float raw_focus = beta_theta_ratio * 10.0f + smr_contribution;

    // Normalize to 0-100
    float focus = (raw_focus - 5.0f) / 45.0f * 100.0f;

    if (focus < 0) focus = 0;
    if (focus > 100) focus = 100;

    return focus;
}

static float calculate_relaxation_index(band_power_t *power) {
    float denominator = power->beta + power->theta;
    if (denominator < 0.001f) denominator = 0.001f;

    float alpha_ratio = power->alpha / denominator;
    float alpha_relative = power->alpha / (power->total + 0.001f);

    float raw_relaxation = alpha_ratio * 0.7f + alpha_relative * 100.0f * 0.3f;
    float relaxation = raw_relaxation * 20.0f;

    if (relaxation < 0) relaxation = 0;
    if (relaxation > 100) relaxation = 100;

    return relaxation;
}

static float calculate_fatigue_index(band_power_t *power) {
    float fast_power = power->alpha + power->beta;
    if (fast_power < 0.001f) fast_power = 0.001f;

    float slow_fast_ratio = (power->theta + power->delta) / fast_power;
    float theta_alpha = power->theta / (power->alpha + 0.001f);

    float raw_fatigue = slow_fast_ratio * 0.5f + theta_alpha * 0.5f;
    float fatigue = raw_fatigue * 30.0f;

    if (fatigue < 0) fatigue = 0;
    if (fatigue > 100) fatigue = 100;

    return fatigue;
}

static float calculate_meditation_index(band_power_t *power_ch1, band_power_t *power_ch2) {
    float avg_alpha = (power_ch1->alpha + power_ch2->alpha) / 2.0f;
    float avg_theta = (power_ch1->theta + power_ch2->theta) / 2.0f;
    float avg_total = (power_ch1->total + power_ch2->total) / 2.0f;

    float alpha_relative = avg_alpha / (avg_total + 0.001f);
    float theta_relative = avg_theta / (avg_total + 0.001f);

    // Simplified coherence (power similarity)
    float alpha_diff = fabsf(power_ch1->alpha - power_ch2->alpha);
    float alpha_sum = power_ch1->alpha + power_ch2->alpha + 0.001f;
    float alpha_coherence = 1.0f - (alpha_diff / alpha_sum);

    float raw_meditation = alpha_relative * 100.0f * 0.4f +
                          theta_relative * 100.0f * 0.3f +
                          alpha_coherence * 100.0f * 0.3f;

    if (raw_meditation < 0) raw_meditation = 0;
    if (raw_meditation > 100) raw_meditation = 100;

    return raw_meditation;
}

// ============================================================================
// Global State (2 channels)
// ============================================================================
static dc_removal_t dc_removal[2];
static notch_filter_t notch[2];
static bandpass_filter_t bandpass[2];
static artifact_detector_t artifact[2];
static moving_avg_t smooth[2];
static fft_processor_t fft[2];
static band_power_t band_power[2];

// Index smoothing
static ema_filter_t focus_ema;
static ema_filter_t relaxation_ema;
static ema_filter_t fatigue_ema;
static ema_filter_t meditation_ema;

// Current indices
static signal_indices_t current_indices;
static uint8_t indices_updated_flag = 0;
static float notch_freq = 50.0f;

// ============================================================================
// Public API Implementation
// ============================================================================

void signal_init(void) {
    for (int ch = 0; ch < 2; ch++) {
        dc_removal_init(&dc_removal[ch]);

        if (notch_freq == 60.0f) {
            notch_init_60hz(&notch[ch]);
        } else {
            notch_init_50hz(&notch[ch]);
        }

        bandpass_init(&bandpass[ch]);
        artifact_init(&artifact[ch]);
        moving_avg_init(&smooth[ch]);
        fft_init(&fft[ch]);

        memset(&band_power[ch], 0, sizeof(band_power_t));
    }

    // Index smoothing with heavy alpha (0.1 = slow response)
    ema_init(&focus_ema, 0.1f);
    ema_init(&relaxation_ema, 0.1f);
    ema_init(&fatigue_ema, 0.1f);
    ema_init(&meditation_ema, 0.1f);

    memset(&current_indices, 0, sizeof(signal_indices_t));
    indices_updated_flag = 0;
}

float signal_process(int32_t raw_sample, uint8_t channel) {
    if (channel > 1) return 0;

    // Convert to float (assuming 24-bit ADC, scale to uV)
    // Adjust scale factor based on your ADC gain settings
    float sample = (float)raw_sample * 0.0223f;  // Example scale to uV

    // DC removal
    sample = dc_remove(&dc_removal[channel], sample);

    // Notch filter (50/60 Hz)
    sample = notch_filter(&notch[channel], sample);

    // Bandpass filter (0.5-50 Hz)
    sample = bandpass_filter(&bandpass[channel], sample);

    // Artifact detection
    detect_artifact(&artifact[channel], sample);

    // Add to FFT buffer
    fft_add_sample(&fft[channel], sample);

    // Check if FFT should be computed (trigger on channel 0)
    if (channel == 0 && fft_ready(&fft[0])) {
        // Compute FFT for both channels
        fft_compute(&fft[0]);
        fft_compute(&fft[1]);

        // Calculate band powers
        calculate_band_power(&fft[0], &band_power[0]);
        calculate_band_power(&fft[1], &band_power[1]);

        // Calculate raw indices
        float focus_raw = calculate_focus_index(&band_power[0]);
        float relax_raw = calculate_relaxation_index(&band_power[0]);
        float fatigue_raw = calculate_fatigue_index(&band_power[0]);
        float meditation_raw = calculate_meditation_index(&band_power[0], &band_power[1]);

        // Smooth indices
        current_indices.focus = ema_filter(&focus_ema, focus_raw);
        current_indices.relaxation = ema_filter(&relaxation_ema, relax_raw);
        current_indices.fatigue = ema_filter(&fatigue_ema, fatigue_raw);
        current_indices.meditation = ema_filter(&meditation_ema, meditation_raw);

        indices_updated_flag = 1;
    }

    // Smoothing for display
    sample = moving_avg(&smooth[channel], sample);

    return sample;
}

uint8_t signal_is_artifact(uint8_t channel) {
    if (channel > 1) return 0;
    return artifact[channel].artifact_detected;
}

void signal_get_band_power(uint8_t channel, band_power_t *power) {
    if (channel > 1) return;
    *power = band_power[channel];
}

void signal_get_indices(signal_indices_t *indices) {
    *indices = current_indices;
}

uint8_t signal_indices_updated(void) {
    if (indices_updated_flag) {
        indices_updated_flag = 0;
        return 1;
    }
    return 0;
}

void signal_set_notch_freq(float freq) {
    notch_freq = freq;
    for (int ch = 0; ch < 2; ch++) {
        if (freq == 60.0f) {
            notch_init_60hz(&notch[ch]);
        } else {
            notch_init_50hz(&notch[ch]);
        }
    }
}

void signal_set_smoothing(float alpha) {
    // Could be used to adjust EMA smoothing
    (void)alpha;
}
