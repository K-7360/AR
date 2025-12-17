/**
 *  ADS1299 EEG Analog Frontend - Simplified Header
 *  Register definitions and function prototypes
 */

#ifndef ADS1299_H
#define ADS1299_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration */
#define ADS1299_MAX_CHANNELS    8
#define ADS1299_SAMPLE_RATE     250

/* Sign extension for 24-bit to 32-bit */
#define ADS1299_SIGN_EXT_24(VAL)    ((int32_t)((int32_t)((VAL) << 8) >> 8))

/* Device ID - check bits 4-0 only (REV_ID in bits 7-5 may vary) */
#define ADS1299_ID_MASK         0x1F    // Mask for bits 4-0
#define ADS1299_ID_8CH          0x1E    // xxx1 1110: bit4=1, DEV_ID=11, NU_CH=10 (8-ch)
#define ADS1299_ID_6CH          0x1D    // xxx1 1101: bit4=1, DEV_ID=11, NU_CH=01 (6-ch)
#define ADS1299_ID_4CH          0x1C    // xxx1 1100: bit4=1, DEV_ID=11, NU_CH=00 (4-ch)

/* Register Addresses */
#define ADS1299_REG_ID          0x00
#define ADS1299_REG_CONFIG1     0x01
#define ADS1299_REG_CONFIG2     0x02
#define ADS1299_REG_CONFIG3     0x03
#define ADS1299_REG_LOFF        0x04
#define ADS1299_REG_CH1SET      0x05
#define ADS1299_REG_CH2SET      0x06
#define ADS1299_REG_CH3SET      0x07
#define ADS1299_REG_CH4SET      0x08
#define ADS1299_REG_CH5SET      0x09
#define ADS1299_REG_CH6SET      0x0A
#define ADS1299_REG_CH7SET      0x0B
#define ADS1299_REG_CH8SET      0x0C
#define ADS1299_REG_GPIO        0x14
#define ADS1299_REG_CONFIG4     0x17

/* SPI Commands */
#define ADS1299_CMD_WAKEUP      0x02
#define ADS1299_CMD_STANDBY     0x04
#define ADS1299_CMD_RESET       0x06
#define ADS1299_CMD_START       0x08
#define ADS1299_CMD_STOP        0x0A
#define ADS1299_CMD_RDATAC      0x10
#define ADS1299_CMD_SDATAC      0x11
#define ADS1299_CMD_RDATA       0x12
#define ADS1299_CMD_RREG        0x20
#define ADS1299_CMD_WREG        0x40
#define ADS1299_CMD_NOP         0xFF

/* CONFIG1 Register */
#define ADS1299_CONFIG1_RESERVED        ((1<<7)|(1<<4))
#define ADS1299_CONFIG1_DR_250SPS       6

/* CONFIG2 Register */
#define ADS1299_CONFIG2_RESERVED        (6<<5)
#define ADS1299_CONFIG2_CAL_INT         (1<<4)
#define ADS1299_CONFIG2_CAL_FREQ_1HZ    0

/* CONFIG3 Register */
#define ADS1299_CONFIG3_RESERVED        (3<<5)
#define ADS1299_CONFIG3_REFBUF_EN       (1<<7)
#define ADS1299_CONFIG3_BIASREF_INT     (1<<3)
#define ADS1299_CONFIG3_BIASBUF_EN      (1<<2)

/* Channel Settings */
#define ADS1299_CHNSET_PD_ON            (0<<7)
#define ADS1299_CHNSET_PD_OFF           (1<<7)
#define ADS1299_CHNSET_GAIN_24          (6<<4)
#define ADS1299_CHNSET_SRB2_OPEN        (0<<3)
#define ADS1299_CHNSET_MUX_NORMAL       0

/* Function Prototypes */
void ads1299_init(void);
void ads1299_send_cmd(uint8_t cmd);
void ads1299_write_reg(uint8_t reg_addr, uint8_t value);
uint8_t ads1299_read_reg(uint8_t reg_addr);
void ads1299_start_rdatac(void);
void ads1299_stop_rdatac(void);
void ads1299_start_conversion(void);
void ads1299_stop_conversion(void);

#ifdef __cplusplus
}
#endif

#endif /* ADS1299_H */
