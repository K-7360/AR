/**
 * @file synthetic_data.cpp
 * @brief Synthetic EEG Data Generator Implementation
 */

#include "synthetic_data.h"
#include <math.h>
#include <stdlib.h>

#define SAMPLE_RATE_HZ      250.0f
#define PI                  3.14159265f

static uint32_t sample_count = 0;
static float index_values[4] = {72.0f, 45.0f, 30.0f, 58.0f};

static float random_float(float min, float max) {
    float scale = rand() / (float) RAND_MAX;
    return min + scale * (max - min);
}

void synthetic_init(void) {
    sample_count = 0;
    srand(12345);
}

float synthetic_get_ch1_sample(void) {
    float t = sample_count / SAMPLE_RATE_HZ;
    float alpha = 50.0f * sinf(2.0f * PI * 10.0f * t);
    float drift = 20.0f * sinf(2.0f * PI * 0.5f * t);
    float noise = random_float(-10.0f, 10.0f);
    sample_count++;
    return alpha + drift + noise;
}

float synthetic_get_ch2_sample(void) {
    float t = (sample_count - 1) / SAMPLE_RATE_HZ;
    float beta = 40.0f * sinf(2.0f * PI * 20.0f * t);
    float drift = 15.0f * sinf(2.0f * PI * 0.3f * t);
    float noise = random_float(-8.0f, 8.0f);
    return beta + drift + noise;
}

float synthetic_get_index(uint8_t index_id) {
    if (index_id >= 4) return 50.0f;

    // Random walk: add small random change each call
    index_values[index_id] += random_float(-3.0f, 3.0f);

    // Clamp to 10-90 range
    if (index_values[index_id] < 10.0f) index_values[index_id] = 10.0f;
    if (index_values[index_id] > 90.0f) index_values[index_id] = 90.0f;

    return index_values[index_id];
}
