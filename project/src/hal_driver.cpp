/*
 * @Description: About API function of hardware layer
 * @version: 1.4
 * @Autor: lmx
 * @LastEditors: lmx
 */
#include "hal_driver.h"
#include "jbd013_api.h"
#include "SPI.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

// DMA buffer for fast SPI transfers
static u8* dma_buffer = NULL;
static const u32 DMA_CHUNK_SIZE = 64000;  // 64KB chunks

/**
 * @description: Delay N us
 * @param {u32} val: delay time, unit: us
 */
void delay_us(u32 val)
{
    delayMicroseconds(val);
}

/**
 * @description: Delay N ms
 * @param {u32} val: delay time, unit: ms
 */
void delay_ms(u32 val)
{
    delay_us(val * 1000);
}

/**
 * @description: Set spi_cs pin status
 * @param {u8} val: spi_cs pin status bit
 */
void set_spi_cs_pin(u8 val)
{
    digitalWrite(15, val);  // GPIO 15 for JBD013VGA CS
}

/**
 * @description: Send a byte of data
 * @param {u8} param: Data sent
 */
void spi_tx_byte(u8 param)
{
    SPI.write(param);
}

/**
 * @description: Receive a byte of data
 * @return {u8}: Received data
 */
u8 spi_rx_byte(void)
{
    uint8_t data = SPI.transfer(0x00);
    return data;
}

/**
 * @description: Write a byte of data
 * @param {u8} param: Data written
 */
void spi_wr_byte(u8 param)
{
    set_spi_cs_pin(SET_LOW);
    spi_tx_byte(param);
    set_spi_cs_pin(SET_HIGH);
}

/**
 * @description: Read a byte of data
 * @return {u8}: Data read
 */
u8 spi_rd_byte(u8 cmd)
{
    u8 ret;

    set_spi_cs_pin(SET_LOW);
    spi_tx_byte(cmd);
    ret = spi_rx_byte();
    set_spi_cs_pin(SET_HIGH);

    return ret;
}

/**
 * @description: Write multiple bytes data
 * @param {u8} cmd: SPI instruction of JBD013VGA panel
 * @param {u8} *pBuf: Pointer to write data
 * @param {u32} len: Length of data written
 */
void spi_wr_bytes(u8 cmd, u8 *pBuf, u32 len)
{
    u32 i;

    set_spi_cs_pin(SET_LOW);
    spi_tx_byte(cmd);
    for (i = 0; i < len; i++)
    {
        spi_tx_byte(pBuf[i]);
    }
    set_spi_cs_pin(SET_HIGH);
}

/**
 * @description: Read multiple bytes data
 * @param {u8} cmd: SPI instruction of JBD013VGA panel
 * @param {u8} *pBuf: Pointer to receive data
 * @param {u32} len: Length of received data
 */
void spi_rd_bytes(u8 cmd, u8 *pBuf, u32 len)
{
    u32 i;

    set_spi_cs_pin(SET_LOW);
    spi_tx_byte(cmd);
    for (i = 0; i < len; i++)
    {
        pBuf[i] = spi_rx_byte();
    }

    set_spi_cs_pin(SET_HIGH);
}

/**
 * @description: Read data from the panel cache
 * @param {u16} col: The starting column address of the display area (0~639)
 * @param {u16} row: The starting row address of the display area (0~479)
 * @param {u8} *pBuf: Pointer to receive data
 *  Gray: It is the gray data of pixel, which is composed of 4 bits
 *  pBuf[N] = GrayN << 4 | GrayN+1
 * @param {u32} len: Length of received data (MaxLen=640*480/2=153600)
 */
void spi_rd_cache(u16 col, u16 row, u8 *pBuf, u32 len)
{
    u32 i;
    u32 addr;

    addr = ((row & 0x1ff) << 10) | (col & 0x3ff);
    set_spi_cs_pin(SET_LOW);
    spi_tx_byte(SPI_RD_CACHE);     // CMD
    spi_tx_byte((u8)(addr >> 16)); // Addr
    spi_tx_byte((u8)(addr >> 8));  // Addr
    spi_tx_byte((u8)(addr));       // Addr
    spi_tx_byte(0xff);             // Dummy
    for (i = 0; i < len; i++)      // Pixel data
    {
        pBuf[i] = spi_rx_byte();
    }
    set_spi_cs_pin(SET_HIGH);
}

/**
 * @description: Write data to the cache in the panel
 * @param {u16} col: The starting column address of the display area (0~639)
 * @param {u16} row: The starting row address of the display area (0~479)
 * @param {u8} *pBuf: Pointer to write data
 *  Gray: It is the gray data of pixel, which is composed of 4 bits
 *  pBuf[N] = GrayN << 4 | GrayN+1
 * @param {u32} len: Length of send data (MaxLen=640*480/2=153600)
 */
void spi_wr_cache(u16 col, u16 row, u8 *pBuf, u32 len)
{
    u32 addr;

    // Note: spi_rd_cache removed - write-only mode (no MISO connected)
    // This may cause minor display artifacts at write boundaries
    // but avoids GPIO 12 boot conflict on ESP32-PICO-D4

    addr = ((row & 0x1ff) << 10) | (col & 0x3ff);
    set_spi_cs_pin(SET_LOW);
    spi_tx_byte(SPI_WR_CACHE);     // CMD
    spi_tx_byte((u8)(addr >> 16)); // Addr
    spi_tx_byte((u8)(addr >> 8));  // Addr
    spi_tx_byte((u8)(addr));       // Addr
    spi_tx_byte(0xff);             // Dummy

    // Transfer in larger chunks for better DMA efficiency
    // SPI DMA works best with aligned transfers
    SPI.transferBytes(pBuf, NULL, len);

    spi_tx_byte(0x00); // Write endPixel (dummy, no read available)
    set_spi_cs_pin(SET_HIGH);
}

/**
 * @description: Read the data of the temperature sensor inside the panel
 * @param {u8} sensorId: Temperature sensor ID (Range: 0~3)
 * @param {u8} *pBuf: Data buf
 * @param {u16} len: Length of pbuf (More than 2000 recommended)
 */
void spi_rd_temperature_sensor(u8 sensorId, u8 *pBuf, u16 len)
{
    u16 i;

    set_spi_cs_pin(SET_LOW);
    spi_tx_byte(SPI_RD_TEMP_SENSOR); // CMD
    spi_tx_byte(sensorId);           // sensorId
    spi_tx_byte(0);                  // dummy data
    spi_tx_byte(0);                  // dummy data
    for (i = 0; i < len; i++)        // Data buf
    {
        pBuf[i] = spi_rx_byte();
    }
    set_spi_cs_pin(SET_HIGH);
}
