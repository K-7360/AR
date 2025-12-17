/**
 * @file synthetic_data.h
 * @brief Synthetic EEG Data Generator
 */

#ifndef SYNTHETIC_DATA_H
#define SYNTHETIC_DATA_H

#include <stdint.h>

void synthetic_init(void);
float synthetic_get_ch1_sample(void);
float synthetic_get_ch2_sample(void);
float synthetic_get_index(uint8_t index_id);

#endif
