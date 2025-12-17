/**
 *  ADS1299 ESP32-S2 HAL Implementation
 *  Dedicated SPI bus (HSPI/SPI3) with DRDY interrupt and double buffer
 */

#include "ads1299_esp32s2.h"
#include "ads1299.h"
#include "ads1299_hal.h"
#include <Arduino.h>
#include <SPI.h>

/* Pin Definitions - ADS1299 for ESP32-PICO-D4 */
#define ADS1299_PIN_SCLK    18      /* ESP_SCK1 */
#define ADS1299_PIN_MOSI    23      /* ESP_MOSI1 → ADS1299 DIN */
#define ADS1299_PIN_MISO    19      /* ESP_MISO1 ← ADS1299 DOUT */
#define ADS1299_PIN_CS      5       /* ESP_CS1 */
#define ADS1299_PIN_DRDY    27      /* ADS_DRDY_ESP */
#define ADS1299_PIN_PWDN    25      /* PWDN (works in mode 'a') */
#define ADS1299_PIN_START   26      /* ADS_START_ESP */
#define ADS1299_PIN_RESET   -1      /* Not connected */

#define ADS1299_SPI_SPEED   1000000  /* 1 MHz */

/* Double Buffer Structure */
typedef struct {
    int32_t ch1[ADS1299_BUFFER_SIZE];
    int32_t ch2[ADS1299_BUFFER_SIZE];
    volatile uint16_t write_idx;
    volatile bool ready;
} ads1299_buffer_t;

static ads1299_buffer_t g_buf[2];
static volatile uint8_t g_write_buf = 0;
static volatile uint32_t g_sample_count = 0;
static volatile uint32_t g_missed_samples = 0;
static volatile bool g_drdy_flag = false;
static bool g_connected = false;

static SPIClass *g_spi = NULL;

/* Sign extend 24-bit to 32-bit */
static inline int32_t sign_extend_24(uint32_t val)
{
    return (int32_t)((int32_t)(val << 8) >> 8);
}

/* HAL Functions */
static uint8_t hal_spi_transfer(uint8_t data)
{
    return g_spi->transfer(data);
}

static void hal_cs_set(uint8_t level)
{
    digitalWrite(ADS1299_PIN_CS, level ? HIGH : LOW);
}

static void hal_reset_set(uint8_t level)
{
    /* Use PWDN pin for reset/enable control */
    if (ADS1299_PIN_PWDN >= 0) {
        digitalWrite(ADS1299_PIN_PWDN, level ? HIGH : LOW);
    }
}

static uint8_t hal_drdy_get(void)
{
    return digitalRead(ADS1299_PIN_DRDY);
}

static void hal_delay_us(uint32_t us)
{
    delayMicroseconds(us);
}

static void hal_delay_ms(uint32_t ms)
{
    delay(ms);
}

static void hal_spi_begin(void)
{
    g_spi->beginTransaction(SPISettings(ADS1299_SPI_SPEED, MSBFIRST, SPI_MODE1));
}

static void hal_spi_end(void)
{
    g_spi->endTransaction();
}

static const ads1299_hal_t g_hal = {
    .spi_transfer = hal_spi_transfer,
    .cs_set = hal_cs_set,
    .reset_set = hal_reset_set,
    .drdy_get = hal_drdy_get,
    .delay_us = hal_delay_us,
    .delay_ms = hal_delay_ms,
    .spi_begin = hal_spi_begin,
    .spi_end = hal_spi_end,
};

/* DRDY Interrupt Handler - only sets flag */
static void IRAM_ATTR onDrdyInterrupt(void)
{
    g_drdy_flag = true;
}

/* Read sample from ADS1299 and store in buffer */
static void ads1299_read_sample(void)
{
    ads1299_buffer_t *buf = &g_buf[g_write_buf];

    if (buf->write_idx >= ADS1299_BUFFER_SIZE) {
        g_missed_samples++;
        return;
    }

    /* Read data: 3 status + 8 channels × 3 bytes = 27 bytes */
    g_spi->beginTransaction(SPISettings(ADS1299_SPI_SPEED, MSBFIRST, SPI_MODE1));
    digitalWrite(ADS1299_PIN_CS, LOW);

    /* Skip status bytes */
    g_spi->transfer(0xFF);
    g_spi->transfer(0xFF);
    g_spi->transfer(0xFF);

    /* Read CH1 */
    uint32_t ch1_raw = 0;
    ch1_raw |= ((uint32_t)g_spi->transfer(0xFF)) << 16;
    ch1_raw |= ((uint32_t)g_spi->transfer(0xFF)) << 8;
    ch1_raw |= ((uint32_t)g_spi->transfer(0xFF));

    /* Read CH2 */
    uint32_t ch2_raw = 0;
    ch2_raw |= ((uint32_t)g_spi->transfer(0xFF)) << 16;
    ch2_raw |= ((uint32_t)g_spi->transfer(0xFF)) << 8;
    ch2_raw |= ((uint32_t)g_spi->transfer(0xFF));

    /* Skip CH3-CH8 (18 bytes) */
    for (int i = 0; i < 18; i++) {
        g_spi->transfer(0xFF);
    }

    digitalWrite(ADS1299_PIN_CS, HIGH);
    g_spi->endTransaction();

    /* Store sign-extended values */
    buf->ch1[buf->write_idx] = sign_extend_24(ch1_raw);
    buf->ch2[buf->write_idx] = sign_extend_24(ch2_raw);
    buf->write_idx++;
    g_sample_count++;

    /* Buffer full - swap */
    if (buf->write_idx >= ADS1299_BUFFER_SIZE) {
        buf->ready = true;
        g_write_buf = 1 - g_write_buf;
        g_buf[g_write_buf].write_idx = 0;
        g_buf[g_write_buf].ready = false;
    }
}

/* Public Functions */
void ads1299_esp32s2_init(void)
{
    /* Initialize buffers */
    g_buf[0].write_idx = 0;
    g_buf[0].ready = false;
    g_buf[1].write_idx = 0;
    g_buf[1].ready = false;
    g_write_buf = 0;
    g_sample_count = 0;
    g_missed_samples = 0;

    /* Create dedicated SPI instance (HSPI) */
    g_spi = new SPIClass(HSPI);
    g_spi->begin(ADS1299_PIN_SCLK, ADS1299_PIN_MISO, ADS1299_PIN_MOSI, ADS1299_PIN_CS);

    /* Configure pins */
    pinMode(ADS1299_PIN_CS, OUTPUT);
    digitalWrite(ADS1299_PIN_CS, HIGH);

    pinMode(ADS1299_PIN_DRDY, INPUT);

    /* PWDN pin (Power Down / Enable) */
    if (ADS1299_PIN_PWDN >= 0) {
        pinMode(ADS1299_PIN_PWDN, OUTPUT);
        digitalWrite(ADS1299_PIN_PWDN, HIGH);  /* HIGH = Power ON */
    }

    /* START pin */
    if (ADS1299_PIN_START >= 0) {
        pinMode(ADS1299_PIN_START, OUTPUT);
        digitalWrite(ADS1299_PIN_START, LOW);  /* LOW = Stop conversions initially */
    }

    /* Initialize HAL */
    ads1299_hal_init(&g_hal);

    /* Initialize ADS1299 */
    ads1299_init();

    /* Verify device ID (mask off REV_ID bits 7-5) */
    uint8_t id = ads1299_read_reg(ADS1299_REG_ID);
    uint8_t id_masked = id & ADS1299_ID_MASK;
    /* Accept 0x1C-0x1F as valid (some chips may have slight variations) */
    g_connected = (id_masked >= ADS1299_ID_4CH && id_masked <= 0x1F);
    Serial.printf("ADS1299 ID: 0x%02X (masked: 0x%02X) %s\n", id, id_masked, g_connected ? "OK" : "NOT FOUND");
}

void ads1299_esp32s2_start_sampling(void)
{
    g_drdy_flag = false;

    /* Start continuous read mode and conversions */
    ads1299_start_rdatac();
    ads1299_start_conversion();

    /* Attach DRDY interrupt (falling edge - active low) */
    attachInterrupt(digitalPinToInterrupt(ADS1299_PIN_DRDY), onDrdyInterrupt, FALLING);
}

void ads1299_esp32s2_stop_sampling(void)
{
    detachInterrupt(digitalPinToInterrupt(ADS1299_PIN_DRDY));
    ads1299_stop_conversion();
    ads1299_stop_rdatac();
    g_drdy_flag = false;
}

void ads1299_esp32s2_process(void)
{
    if (g_drdy_flag) {
        g_drdy_flag = false;
        ads1299_read_sample();
    }
}

bool ads1299_esp32s2_buffer_ready(void)
{
    return g_buf[1 - g_write_buf].ready;
}

bool ads1299_esp32s2_get_buffer(int32_t **ch1, int32_t **ch2, uint16_t *count)
{
    uint8_t read_buf = 1 - g_write_buf;
    ads1299_buffer_t *buf = &g_buf[read_buf];

    if (!buf->ready) {
        return false;
    }

    *ch1 = buf->ch1;
    *ch2 = buf->ch2;
    *count = ADS1299_BUFFER_SIZE;

    buf->ready = false;
    return true;
}

uint32_t ads1299_esp32s2_get_sample_count(void)
{
    return g_sample_count;
}

uint32_t ads1299_esp32s2_get_missed_samples(void)
{
    return g_missed_samples;
}

bool ads1299_esp32s2_is_connected(void)
{
    return g_connected;
}
