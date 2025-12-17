/**
 *  ADS1299 Hardware Abstraction Layer Interface
 */

#ifndef ADS1299_HAL_H
#define ADS1299_HAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HAL function pointer structure */
typedef struct {
    uint8_t (*spi_transfer)(uint8_t data);
    void (*cs_set)(uint8_t level);
    void (*reset_set)(uint8_t level);
    uint8_t (*drdy_get)(void);
    void (*delay_us)(uint32_t us);
    void (*delay_ms)(uint32_t ms);
    void (*spi_begin)(void);
    void (*spi_end)(void);
} ads1299_hal_t;

/* Initialize HAL with platform-specific functions */
void ads1299_hal_init(const ads1299_hal_t *hal);

#ifdef __cplusplus
}
#endif

#endif /* ADS1299_HAL_H */
