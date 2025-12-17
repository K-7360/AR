/**
 *  ADS1299 EEG Analog Frontend - Core Implementation
 *  Platform-independent using HAL
 */

#include "ads1299.h"
#include "ads1299_hal.h"

/* HAL instance */
static const ads1299_hal_t *g_hal = nullptr;

/* Internal helpers */
static inline void cs_low(void) { g_hal->cs_set(0); }
static inline void cs_high(void) { g_hal->cs_set(1); }

void ads1299_hal_init(const ads1299_hal_t *hal)
{
    g_hal = hal;
}

void ads1299_send_cmd(uint8_t cmd)
{
    g_hal->spi_begin();
    cs_low();
    g_hal->spi_transfer(cmd);
    g_hal->delay_us(1);
    cs_high();
    g_hal->spi_end();
}

void ads1299_write_reg(uint8_t reg_addr, uint8_t value)
{
    g_hal->spi_begin();
    cs_low();
    g_hal->spi_transfer(ADS1299_CMD_WREG | reg_addr);
    g_hal->delay_us(1);
    g_hal->spi_transfer(0x00);  // Write 1 register
    g_hal->delay_us(1);
    g_hal->spi_transfer(value);
    g_hal->delay_us(1);
    cs_high();
    g_hal->spi_end();
}

uint8_t ads1299_read_reg(uint8_t reg_addr)
{
    uint8_t value;
    g_hal->spi_begin();
    cs_low();
    g_hal->delay_us(2);  // tCSS: CS to SCLK setup time
    g_hal->spi_transfer(ADS1299_CMD_RREG | reg_addr);
    g_hal->delay_us(4);  // tCSDECODE: 4 tCLK cycles minimum
    g_hal->spi_transfer(0x00);  // Number of registers - 1
    g_hal->delay_us(4);  // Wait for data ready
    value = g_hal->spi_transfer(0x00);  // Read register value
    g_hal->delay_us(2);
    cs_high();
    g_hal->spi_end();
    return value;
}

void ads1299_start_rdatac(void)
{
    ads1299_send_cmd(ADS1299_CMD_RDATAC);
}

void ads1299_stop_rdatac(void)
{
    ads1299_send_cmd(ADS1299_CMD_SDATAC);
}

void ads1299_start_conversion(void)
{
    ads1299_send_cmd(ADS1299_CMD_START);
}

void ads1299_stop_conversion(void)
{
    ads1299_send_cmd(ADS1299_CMD_STOP);
}

static void ads1299_config_channel(uint8_t channel, uint8_t enabled)
{
    uint8_t reg_addr = ADS1299_REG_CH1SET + (channel - 1);
    uint8_t value;

    if (enabled) {
        value = ADS1299_CHNSET_PD_ON | ADS1299_CHNSET_GAIN_24 |
                ADS1299_CHNSET_SRB2_OPEN | ADS1299_CHNSET_MUX_NORMAL;
    } else {
        value = ADS1299_CHNSET_PD_OFF;
    }
    ads1299_write_reg(reg_addr, value);
}

void ads1299_init(void)
{
    /* Hardware reset */
    if (g_hal->reset_set) {
        g_hal->reset_set(0);
        g_hal->delay_ms(1);
        g_hal->reset_set(1);
        g_hal->delay_ms(150);
    }

    /* Stop continuous mode and conversions */
    ads1299_stop_rdatac();
    ads1299_stop_conversion();

    /* CONFIG1: 250 SPS */
    ads1299_write_reg(ADS1299_REG_CONFIG1,
        ADS1299_CONFIG1_RESERVED | ADS1299_CONFIG1_DR_250SPS);

    /* CONFIG2: Internal test signal */
    ads1299_write_reg(ADS1299_REG_CONFIG2,
        ADS1299_CONFIG2_RESERVED | ADS1299_CONFIG2_CAL_INT |
        ADS1299_CONFIG2_CAL_FREQ_1HZ);

    /* CONFIG3: Enable internal reference and bias */
    ads1299_write_reg(ADS1299_REG_CONFIG3,
        ADS1299_CONFIG3_RESERVED | ADS1299_CONFIG3_REFBUF_EN |
        ADS1299_CONFIG3_BIASREF_INT | ADS1299_CONFIG3_BIASBUF_EN);

    /* Wait for reference to settle */
    g_hal->delay_ms(150);

    /* GPIO: All outputs, all low */
    ads1299_write_reg(ADS1299_REG_GPIO, 0x00);

    /* Enable CH1 and CH2, power down CH3-CH8 */
    ads1299_config_channel(1, 1);
    ads1299_config_channel(2, 1);
    for (uint8_t ch = 3; ch <= 8; ch++) {
        ads1299_config_channel(ch, 0);
    }
}
