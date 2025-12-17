/**
 *  ADS1299 ESP32-S2 HAL Implementation Header
 *  Dedicated SPI bus (HSPI) with DRDY interrupt and double buffer
 */

#ifndef ADS1299_ESP32S2_H
#define ADS1299_ESP32S2_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADS1299_BUFFER_SIZE 64  /* Samples per buffer (~256ms at 250Hz) */

/* Initialize ADS1299 HAL for ESP32-S2 */
void ads1299_esp32s2_init(void);

/* Start interrupt-driven sampling */
void ads1299_esp32s2_start_sampling(void);

/* Stop interrupt-driven sampling */
void ads1299_esp32s2_stop_sampling(void);

/* Process pending DRDY - call frequently from main loop */
void ads1299_esp32s2_process(void);

/* Check if a buffer is ready for processing */
bool ads1299_esp32s2_buffer_ready(void);

/* Get the ready buffer for processing */
bool ads1299_esp32s2_get_buffer(int32_t **ch1, int32_t **ch2, uint16_t *count);

/* Get sample statistics */
uint32_t ads1299_esp32s2_get_sample_count(void);
uint32_t ads1299_esp32s2_get_missed_samples(void);

/* Check if ADS1299 hardware is detected */
bool ads1299_esp32s2_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* ADS1299_ESP32S2_H */
