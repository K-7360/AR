#include <Arduino.h>
#include <SPI.h>
#include <math.h>

// Set to 0 for synthetic data
// Set to 1 for signal processing with dummy input
// Set to 2 for real ADC input with timer interrupt
// Set to 3 for ADS1299 EEG frontend (dedicated SPI + DRDY interrupt)
#define USE_SIGNAL_PROCESSING 3

#include "jbd013_api.h"
#include "UI/ui_manager.h"
#include "UI/synthetic_data.h"
#include "SigProc/signal_processing.h"

#if USE_SIGNAL_PROCESSING == 3
#include "ADS1299/ads1299_esp32s2.h"
#include "ADS1299/ads1299.h"
// #include "ADS1299/ads1299_test.h"  // 已移除依赖
#endif

// SPI pin definitions (JBD013VGA Display)
#define SPI_CLK 14
#define SPI_MISO -1   // Not used (GPIO 12 conflicts with ESP32-PICO boot)
#define SPI_MOSI 13
#define SPI_CS 15

// ADC pin definitions (not used with ADS1299)
// #define ADC_CH1_PIN 1
// #define ADC_CH2_PIN 2

// Timing
#define SAMPLE_RATE_HZ 250
#define SAMPLE_INTERVAL_US (1000000 / SAMPLE_RATE_HZ)  // 4000 us

// UI state
ui_state_t ui;

// Timing variables
unsigned long last_sample_time = 0;
unsigned long last_index_update = 0;
unsigned long last_fps_time = 0;
uint32_t frame_count = 0;

#if USE_SIGNAL_PROCESSING == 2
// ============================================================================
// Timer-based ADC sampling with double buffer
// ============================================================================
#define ADC_BUFFER_SIZE 64  // Samples per buffer (256ms worth at 250Hz)

// Double buffer structure
typedef struct {
    int32_t ch1[ADC_BUFFER_SIZE];
    int32_t ch2[ADC_BUFFER_SIZE];
    volatile uint16_t write_idx;
    volatile bool ready;
} adc_buffer_t;

static adc_buffer_t adc_buf[2];  // Double buffer
static volatile uint8_t adc_write_buf = 0;  // Which buffer ISR writes to
static volatile uint8_t adc_read_buf = 1;   // Which buffer main loop reads from

// Hardware timer
hw_timer_t *adc_timer = NULL;

// Timer ISR - samples ADC at exactly 250 Hz
void IRAM_ATTR onAdcTimer() {
    adc_buffer_t *buf = &adc_buf[adc_write_buf];

    if (buf->write_idx < ADC_BUFFER_SIZE) {
        // Read ADC (centered at 2048)
        buf->ch1[buf->write_idx] = analogRead(ADC_CH1_PIN) - 2048;
        buf->ch2[buf->write_idx] = analogRead(ADC_CH2_PIN) - 2048;
        buf->write_idx++;

        // Buffer full - swap buffers
        if (buf->write_idx >= ADC_BUFFER_SIZE) {
            buf->ready = true;
            // Swap write buffer
            adc_write_buf = 1 - adc_write_buf;
            adc_buf[adc_write_buf].write_idx = 0;
            adc_buf[adc_write_buf].ready = false;
        }
    }
}
#endif

void setup() {
    delay(100);

    Serial.begin(115200);
    Serial.println("\n\n=== EEG System Starting ===");
    Serial.println("Press 'a' for ADS1299-only test (serial output)");
    Serial.println("Press 'b' for ADS1299 + Display (waveform test)");
    Serial.println("Press 'c' for Simple Display Test (white screen)");
    Serial.println("Press 'd' for full display mode");
    Serial.println("Waiting 5 seconds...");

    // Wait for user input
    char selected_mode = 'd';  // default
    unsigned long wait_start = millis();
    while (millis() - wait_start < 5000) {
        if (Serial.available()) {
            char cmd = Serial.read();
            if (cmd == 'a' || cmd == 'A') {
                selected_mode = 'a';
                break;
            } else if (cmd == 'b' || cmd == 'B') {
                selected_mode = 'b';
                break;
            } else if (cmd == 'c' || cmd == 'C') {
                selected_mode = 'c';
                break;
            } else if (cmd == 'd' || cmd == 'D') {
                selected_mode = 'd';
                break;
            }
        }
    }

    // ========================================
    // MODE 'c': Simple Display Test
    // ========================================
    if (selected_mode == 'c') {
        Serial.println("\n[SIMPLE DISPLAY TEST]");
        Serial.println("Testing display with minimal code...\n");
        
        // Configure SPI with LOW speed
        Serial.println("1. Configuring SPI (1MHz, write-only)...");
        SPI.begin(SPI_CLK, -1, SPI_MOSI, SPI_CS);  // MISO = -1
        SPI.setBitOrder(MSBFIRST);
        SPI.setDataMode(SPI_MODE0);
        SPI.setFrequency(1000000);  // 1MHz - very slow
        pinMode(SPI_CS, OUTPUT);
        digitalWrite(SPI_CS, HIGH);
        delay(100);
        Serial.println("   Done.");
        
        // Reset panel
        Serial.println("2. Resetting panel...");
        send_cmd(SPI_RST_EN);
        delay(10);
        send_cmd(SPI_RST);
        delay(100);  // Long delay after reset
        Serial.println("   Done.");
        
        // Enable write
        Serial.println("3. Enabling write...");
        send_cmd(SPI_WR_ENABLE);
        delay(10);
        Serial.println("   Done.");
        
        // Close demura
        Serial.println("4. Closing demura...");
        wr_status_reg(SPI_WR_STATUS_REG1, 0x10);
        delay(10);
        Serial.println("   Done.");
        
        // Set offset and current
        Serial.println("5. Setting offset and current...");
        wr_offset_reg(0, 0);
        wr_offset_reg(0, 20);
        wr_offset_reg(24, 0);
        wr_offset_reg(24, 20);
        wr_offset_reg(12, 10);
        wr_cur_reg(63);
        delay(10);
        Serial.println("   Done.");
        
        // Enable display
        Serial.println("6. Enabling display...");
        send_cmd(SPI_DISPLAY_ENABLE);
        delay(10);
        send_cmd(SPI_SYNC);
        delay(10);
        Serial.println("   Done.");
        
        // Set brightness
        Serial.println("7. Setting brightness (max)...");
        wr_lum_reg(63);  // Maximum brightness
        delay(10);
        Serial.println("   Done.");
        
        // Fill screen with white (test pattern)
        Serial.println("8. Filling screen with WHITE...");
        uint8_t white_line[320];
        memset(white_line, 0xFF, sizeof(white_line));  // 0xFF = white
        
        for (int row = 0; row < 480; row++) {
            spi_wr_cache(0, row, white_line, 320);
            if (row % 60 == 0) {
                Serial.printf("   Row %d/480\n", row);
            }
        }
        send_cmd(SPI_SYNC);
        delay(100);
        Serial.println("   Done.");
        
        Serial.println("\n=== TEST COMPLETE ===");
        Serial.println("Screen should be WHITE now!");
        Serial.println("");
        Serial.println("If screen is still black:");
        Serial.println("  - Check VCC power (3.3V)");
        Serial.println("  - Check all wire connections");
        Serial.println("  - Try pressing reset button");
        
        // Keep running - do nothing
        while (1) {
            delay(1000);
        }
    }

    // ========================================
    // MODE 'a': ADS1299 only (serial output)
    // ========================================
    if (selected_mode == 'a') {
        // ADS1299-only test mode (no display)
        Serial.println("\n[ADS1299-ONLY MODE] Skipping display initialization...");
        
#if USE_SIGNAL_PROCESSING == 3
        // ========== SPI DIAGNOSTIC TEST ==========
        Serial.println("\n=== SPI Diagnostic Test ===");
        
        // Setup pins
        pinMode(5, OUTPUT);   // CS
        pinMode(25, OUTPUT);  // PWDN
        pinMode(26, OUTPUT);  // START
        pinMode(27, INPUT);   // DRDY
        
        digitalWrite(5, HIGH);   // CS high
        digitalWrite(25, LOW);   // PWDN low (device off)
        digitalWrite(26, LOW);   // START low
        delay(100);
        
        // Power up sequence
        Serial.println("Power-up sequence...");
        digitalWrite(25, HIGH);  // PWDN high (enable device)
        delay(500);              // Wait for power-up
        
        // Test with different SPI modes
        SPIClass testSpi(HSPI);
        testSpi.begin(18, 19, 23, 5);  // SCK=18, MISO=19, MOSI=23, CS=5
        
        uint8_t spi_modes[] = {SPI_MODE0, SPI_MODE1, SPI_MODE2, SPI_MODE3};
        const char* mode_names[] = {"MODE0", "MODE1", "MODE2", "MODE3"};
        
        for (int m = 0; m < 4; m++) {
            Serial.printf("\nTesting SPI %s:\n", mode_names[m]);
            
            // Send SDATAC command first
            testSpi.beginTransaction(SPISettings(500000, MSBFIRST, spi_modes[m]));
            digitalWrite(5, LOW);
            delayMicroseconds(10);
            testSpi.transfer(0x11);  // SDATAC
            delayMicroseconds(10);
            digitalWrite(5, HIGH);
            testSpi.endTransaction();
            delay(10);
            
            // Read ID register
            testSpi.beginTransaction(SPISettings(500000, MSBFIRST, spi_modes[m]));
            digitalWrite(5, LOW);
            delayMicroseconds(10);
            testSpi.transfer(0x20);  // RREG | 0x00 (read ID register)
            delayMicroseconds(10);
            testSpi.transfer(0x00);  // Read 1 register
            delayMicroseconds(10);
            uint8_t id = testSpi.transfer(0x00);
            delayMicroseconds(10);
            digitalWrite(5, HIGH);
            testSpi.endTransaction();
            
            Serial.printf("  ID = 0x%02X (expect 0x3E)\n", id);
            
            // Check if this looks correct
            if (id == 0x3E) {
                Serial.printf("  *** %s is CORRECT! ***\n", mode_names[m]);
            } else if (id == 0x1F) {
                Serial.println("  (Right-shifted by 1 bit)");
            } else if (id == 0x7C) {
                Serial.println("  (Left-shifted by 1 bit)");
            }
            
            delay(50);
        }
        
        Serial.println("\n=== End SPI Diagnostic ===\n");
        
        // Now proceed with normal initialization
        // Initialize ADS1299
        ads1299_esp32s2_init();
        
        // Enable test signal
        Serial.println("Enabling 1Hz test signal...");
        ads1299_stop_conversion();
        ads1299_stop_rdatac();
        ads1299_write_reg(ADS1299_REG_CONFIG2, 0xD0);  // 1Hz test signal
        for (uint8_t ch = 0; ch < 8; ch++) {
            ads1299_write_reg(ADS1299_REG_CH1SET + ch, 0x65);  // Test signal input
            delay(5);
        }
        delay(100);
        
        // Control START pin (GPIO 26) - must be HIGH to start conversions
        pinMode(26, OUTPUT);
        digitalWrite(26, HIGH);
        delay(10);
        
        // Start sampling
        ads1299_start_rdatac();
        ads1299_start_conversion();
        
        Serial.println("Reading test signal samples...");
        Serial.println("Format: [sample] CH1, CH2");
        Serial.printf("DRDY pin (GPIO 27) state: %d\n", digitalRead(27));
        
        // Create SPI instance once (outside loop)
        SPIClass *ads_spi = new SPIClass(HSPI);
        ads_spi->begin(18, 19, 23, 5);  // SCK=18, MISO=19, MOSI=23, CS=5
        pinMode(5, OUTPUT);
        digitalWrite(5, HIGH);
        
        // Read and print samples in loop
        uint32_t sample_count = 0;
        while (1) {
            if (digitalRead(27) == LOW) {  // DRDY pin
                // Read data via SPI
                ads_spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));  // 1MHz
                digitalWrite(5, LOW);  // CS low
                
                // Skip status bytes
                ads_spi->transfer(0xFF);
                ads_spi->transfer(0xFF);
                ads_spi->transfer(0xFF);
                
                // Read CH1
                uint32_t ch1_raw = 0;
                ch1_raw |= ((uint32_t)ads_spi->transfer(0xFF)) << 16;
                ch1_raw |= ((uint32_t)ads_spi->transfer(0xFF)) << 8;
                ch1_raw |= ((uint32_t)ads_spi->transfer(0xFF));
                
                // Read CH2
                uint32_t ch2_raw = 0;
                ch2_raw |= ((uint32_t)ads_spi->transfer(0xFF)) << 16;
                ch2_raw |= ((uint32_t)ads_spi->transfer(0xFF)) << 8;
                ch2_raw |= ((uint32_t)ads_spi->transfer(0xFF));
                
                // Skip CH3-CH8
                for (int i = 0; i < 18; i++) ads_spi->transfer(0xFF);
                
                digitalWrite(5, HIGH);  // CS high
                ads_spi->endTransaction();
                
                // Sign extend
                int32_t ch1 = (int32_t)((int32_t)(ch1_raw << 8) >> 8);
                int32_t ch2 = (int32_t)((int32_t)(ch2_raw << 8) >> 8);
                
                // Print raw data for first 10 samples for debugging
                if (sample_count < 10) {
                    Serial.printf("[%6lu] raw: 0x%06X, 0x%06X | signed: %8d, %8d\n", 
                                  sample_count, ch1_raw, ch2_raw, ch1, ch2);
                } else {
                    Serial.printf("[%6lu] %8d, %8d\n", sample_count, ch1, ch2);
                }
                sample_count++;
                
                delay(10);  // Slow down output
            }
        }
#else
        Serial.println("ADS1299 mode not enabled. Set USE_SIGNAL_PROCESSING to 3.");
        while(1) delay(1000);
#endif
    }

    // ========================================
    // MODE 'b': ADS1299 + Display (waveform)
    // ========================================
    if (selected_mode == 'b') {
#if USE_SIGNAL_PROCESSING == 3
        Serial.println("\n[ADS1299 + DISPLAY MODE] Initializing...");
        
        // Step 1: Initialize Display SPI (write-only, no MISO)
        Serial.println("Step 1: Initializing display SPI (write-only)...");
        
        // Note: GPIO 12 (MISO) is NOT used to avoid ESP32-PICO boot conflict
        SPI.begin(SPI_CLK, -1, SPI_MOSI, SPI_CS);  // MISO = -1 (not used)
        SPI.setBitOrder(MSBFIRST);
        SPI.setDataMode(SPI_MODE0);
        SPI.setFrequency(20000000);  // 20MHz
        pinMode(SPI_CS, OUTPUT);
        digitalWrite(SPI_CS, HIGH);
        
        Serial.println("Step 2: Initializing display panel...");
        delay(100);
        
        // Skip reading panel ID (no MISO)
        Serial.println("(Skipping Panel ID read - write-only mode)");
        
        // Reset and init panel
        panel_rst();
        delay(100);
        
        // Open status register write enable
        send_cmd(SPI_WR_ENABLE);
        
        // Close demura
        wr_status_reg(SPI_WR_STATUS_REG1, 0x10);
        
        // Clear cache with simple method
        Serial.println("Clearing display cache...");
        
        // Use smaller buffer for clearing
        uint8_t clear_buf[320];  // Clear 640 pixels (320 bytes) at a time
        memset(clear_buf, 0, sizeof(clear_buf));
        
        for (int row = 0; row < 480; row++) {
            spi_wr_cache(0, row, clear_buf, 320);
            if (row % 48 == 0) {
                Serial.printf("  Clearing row %d/480\n", row);
            }
        }
        
        // Set offset and current
        wr_offset_reg(0, 0);
        wr_offset_reg(0, 20);
        wr_offset_reg(24, 0);
        wr_offset_reg(24, 20);
        wr_offset_reg(12, 10);
        wr_cur_reg(63);
        
        // Enable display
        send_cmd(SPI_DISPLAY_ENABLE);
        send_cmd(SPI_SYNC);
        delay(10);
        
        // Set brightness
        wr_lum_reg(20);
        
        // Set mirror mode
        set_mirror_mode(1);
        
        Serial.println("Display initialized!");
        
        // Step 3: Initialize ADS1299
        Serial.println("Step 3: Initializing ADS1299...");
        
        // Setup ADS1299 pins
        pinMode(5, OUTPUT);   // CS
        pinMode(25, OUTPUT);  // PWDN
        pinMode(26, OUTPUT);  // START
        pinMode(27, INPUT);   // DRDY
        
        digitalWrite(5, HIGH);   // CS high
        digitalWrite(25, LOW);   // PWDN low (device off)
        digitalWrite(26, LOW);   // START low
        delay(100);
        
        // Power up ADS1299
        digitalWrite(25, HIGH);
        delay(500);
        
        // Create ADS1299 SPI instance
        SPIClass *ads_spi = new SPIClass(HSPI);
        ads_spi->begin(18, 19, 23, 5);  // SCK=18, MISO=19, MOSI=23, CS=5
        
        // Send SDATAC
        ads_spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
        digitalWrite(5, LOW);
        delayMicroseconds(10);
        ads_spi->transfer(0x11);  // SDATAC
        delayMicroseconds(10);
        digitalWrite(5, HIGH);
        ads_spi->endTransaction();
        delay(10);
        
        // Read ID
        ads_spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
        digitalWrite(5, LOW);
        delayMicroseconds(10);
        ads_spi->transfer(0x20);  // RREG ID
        delayMicroseconds(10);
        ads_spi->transfer(0x00);
        delayMicroseconds(10);
        uint8_t ads_id = ads_spi->transfer(0x00);
        delayMicroseconds(10);
        digitalWrite(5, HIGH);
        ads_spi->endTransaction();
        
        Serial.printf("ADS1299 ID: 0x%02X %s\n", ads_id, (ads_id == 0x3E) ? "OK" : "ERROR");
        
        if (ads_id != 0x3E) {
            Serial.println("ADS1299 not responding! Check connections.");
            while(1) delay(1000);
        }
        
        // Configure ADS1299 for test signal
        Serial.println("Configuring ADS1299 test signal...");
        
        // Write CONFIG2 for test signal
        ads_spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
        digitalWrite(5, LOW);
        delayMicroseconds(10);
        ads_spi->transfer(0x42);  // WREG CONFIG2
        ads_spi->transfer(0x00);
        ads_spi->transfer(0xD0);  // 1Hz square wave test signal
        digitalWrite(5, HIGH);
        ads_spi->endTransaction();
        delay(10);
        
        // Configure channels for test signal input
        for (int ch = 0; ch < 8; ch++) {
            ads_spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
            digitalWrite(5, LOW);
            delayMicroseconds(10);
            ads_spi->transfer(0x45 + ch);  // WREG CHnSET
            ads_spi->transfer(0x00);
            ads_spi->transfer(0x65);  // Gain=12, Test signal input
            digitalWrite(5, HIGH);
            ads_spi->endTransaction();
            delay(5);
        }
        
        // Start conversions
        digitalWrite(26, HIGH);  // START pin high
        delay(10);
        
        // Send RDATAC
        ads_spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
        digitalWrite(5, LOW);
        delayMicroseconds(10);
        ads_spi->transfer(0x10);  // RDATAC
        delayMicroseconds(10);
        digitalWrite(5, HIGH);
        ads_spi->endTransaction();
        
        // Send START command
        ads_spi->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
        digitalWrite(5, LOW);
        delayMicroseconds(10);
        ads_spi->transfer(0x08);  // START
        delayMicroseconds(10);
        digitalWrite(5, HIGH);
        ads_spi->endTransaction();
        
        Serial.println("ADS1299 configured!");
        Serial.println("\n=== Starting Waveform Display ===");
        
        // Waveform display parameters
        const int WAVE_WIDTH = 600;    // Display width for waveform
        const int WAVE_HEIGHT = 200;   // Height of waveform area
        const int WAVE_Y_CENTER = 240; // Center Y position
        const int WAVE_X_START = 20;   // Left margin
        
        // Waveform buffer
        int16_t wave_buf[WAVE_WIDTH];
        memset(wave_buf, 0, sizeof(wave_buf));
        int wave_idx = 0;
        
        // Line buffer for display
        uint8_t line_buf[320];  // 640 pixels = 320 bytes
        
        uint32_t sample_count = 0;
        uint32_t last_display_time = millis();
        
        while (1) {
            // Read ADS1299 data when DRDY goes low
            if (digitalRead(27) == LOW) {
                ads_spi->beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE1));
                digitalWrite(5, LOW);
                
                // Skip status (3 bytes)
                ads_spi->transfer(0x00);
                ads_spi->transfer(0x00);
                ads_spi->transfer(0x00);
                
                // Read CH1 (3 bytes)
                uint32_t ch1_raw = 0;
                ch1_raw |= ((uint32_t)ads_spi->transfer(0x00)) << 16;
                ch1_raw |= ((uint32_t)ads_spi->transfer(0x00)) << 8;
                ch1_raw |= ((uint32_t)ads_spi->transfer(0x00));
                
                // Skip remaining channels
                for (int i = 0; i < 21; i++) ads_spi->transfer(0x00);
                
                digitalWrite(5, HIGH);
                ads_spi->endTransaction();
                
                // Sign extend
                int32_t ch1 = (int32_t)((int32_t)(ch1_raw << 8) >> 8);
                
                // Scale to display range: ±8388607 -> ±WAVE_HEIGHT/2
                int16_t y_offset = (int16_t)(((int64_t)ch1 * (WAVE_HEIGHT / 2)) / 8388607);
                
                // Store in circular buffer
                wave_buf[wave_idx] = y_offset;
                wave_idx = (wave_idx + 1) % WAVE_WIDTH;
                
                sample_count++;
                
                // Update display every 20ms (50 FPS)
                if (millis() - last_display_time >= 20) {
                    last_display_time = millis();
                    
                    // Clear waveform area and redraw
                    for (int row = WAVE_Y_CENTER - WAVE_HEIGHT/2; row < WAVE_Y_CENTER + WAVE_HEIGHT/2; row++) {
                        memset(line_buf, 0, sizeof(line_buf));
                        
                        // Draw waveform points for this row
                        for (int x = 0; x < WAVE_WIDTH; x++) {
                            int buf_idx = (wave_idx + x) % WAVE_WIDTH;
                            int y_pos = WAVE_Y_CENTER + wave_buf[buf_idx];
                            
                            if (y_pos == row) {
                                // Set pixel (4-bit grayscale, 2 pixels per byte)
                                int px = WAVE_X_START + x;
                                int byte_idx = px / 2;
                                if (px % 2 == 0) {
                                    line_buf[byte_idx] |= 0xF0;  // High nibble
                                } else {
                                    line_buf[byte_idx] |= 0x0F;  // Low nibble
                                }
                            }
                        }
                        
                        // Draw center line (gray)
                        if (row == WAVE_Y_CENTER) {
                            for (int x = 0; x < WAVE_WIDTH; x += 4) {
                                int px = WAVE_X_START + x;
                                int byte_idx = px / 2;
                                line_buf[byte_idx] |= 0x44;  // Dim gray
                            }
                        }
                        
                        spi_wr_cache(0, row, line_buf, 320);
                    }
                    
                    // Sync display
                    send_cmd(SPI_SYNC);
                    
                    // Print status occasionally
                    if (sample_count % 250 == 0) {
                        Serial.printf("Samples: %lu, Last value: %d\n", sample_count, ch1);
                    }
                }
            }
        }
#else
        Serial.println("ADS1299 mode not enabled. Set USE_SIGNAL_PROCESSING to 3.");
        while(1) delay(1000);
#endif
    }

    // ========================================
    // MODE 'd': Full display mode (original)
    // ========================================
    if (selected_mode == 'd') {
        // === Full display mode ===
        Serial.println("\n[FULL MODE] Initializing display...");

    // Configure SPI (write-only, no MISO)
    SPI.begin(SPI_CLK, -1, SPI_MOSI, SPI_CS);  // MISO = -1 (not used)
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    SPI.setFrequency(32000000);
    pinMode(SPI_CS, OUTPUT);
    digitalWrite(SPI_CS, HIGH);

    Serial.println("EEG UI Display Starting...");
    delay_ms(10);

    // Initialize panel
    panel_init();
    clr_cache();
    delay_ms(10);
    wr_lum_reg(20);

    // Set mirror mode (1 = mirror left-right to fix mirrored display)
    set_mirror_mode(1);

    // Initialize UI
    ui_init(&ui);

#if USE_SIGNAL_PROCESSING == 3
    // Initialize ADS1299 EEG frontend
    ads1299_esp32s2_init();

    // Run complete test sequence
    Serial.println("\nADS1299 initialized.");
    Serial.println("Press 'w' to display 1Hz test signal waveform on screen");
    Serial.println("Press 's' to start normal sampling (default after 5s)");

    // Wait for user input
    // 0 = normal, 2 = waveform test signal display
    int run_mode = 0;
    wait_start = millis();  // reuse wait_start variable
    while (millis() - wait_start < 5000) {  // 5 second timeout
        if (Serial.available()) {
            char cmd = Serial.read();
            if (cmd == 's' || cmd == 'S') {
                run_mode = 0;  // Normal mode
                break;
            } else if (cmd == 'w' || cmd == 'W') {
                run_mode = 2;  // Waveform test signal display mode
                break;
            }
        }
    }

    if (run_mode == 2) {
        // Waveform test signal display mode
        // Enable internal test signal (1Hz square wave) on all channels
        Serial.println("\n[TEST SIGNAL MODE] Enabling internal 1Hz test signal...");
        
        // Stop conversions before changing config
        ads1299_stop_conversion();
        ads1299_stop_rdatac();
        
        // CONFIG2 Register (0x02):
        // Bits 7-5: Reserved (110)
        // Bit 4:    INT_TEST = 1 (generate test signals internally)
        // Bits 1-0: TEST_FREQ = 00 (1Hz square wave)
        // Value: 1101 0000 = 0xD0
        ads1299_write_reg(ADS1299_REG_CONFIG2, 0xD0);
        
        // Configure all channels to use test signal input:
        // CHnSET register:
        // Bit 7:    PD = 0 (channel powered on)
        // Bits 6-4: GAIN = 110 (24x)
        // Bit 3:    SRB2 = 0 (open)
        // Bits 2-0: MUX = 101 (test signal)
        // Value: 0110 0101 = 0x65
        for (uint8_t ch = 0; ch < 8; ch++) {
            ads1299_write_reg(ADS1299_REG_CH1SET + ch, 0x65);
            delay(5);
        }
        
        delay(100);

        // Start sampling and display on screen
        ads1299_esp32s2_start_sampling();

        // Initialize signal processing
        signal_init();
        ui_set_signal_status(&ui, "TEST 1Hz");

        // Pre-fill waveform buffer with flat line
        for (int i = 0; i < 2560; i++) {
            ui_add_sample_ch1(&ui, 0);
            ui_add_sample_ch2(&ui, 0);
        }

        Serial.println("[TEST SIGNAL MODE] 1Hz square wave display started");
        Serial.println("You should see a 1Hz square wave on the screen.");
    } else {
        // Normal operation
        ads1299_esp32s2_start_sampling();

        // Initialize signal processing
        signal_init();
        ui_set_signal_status(&ui, "ADS1299");

        // Pre-fill waveform buffer with flat line
        for (int i = 0; i < 2560; i++) {
            ui_add_sample_ch1(&ui, 0);
            ui_add_sample_ch2(&ui, 0);
        }

        Serial.println("ADS1299 sampling started at 250 Hz");
    }
#elif USE_SIGNAL_PROCESSING == 2
    // Initialize ADC for real EEG input
    analogReadResolution(12);  // 12-bit resolution (0-4095)
    analogSetAttenuation(ADC_11db);  // Full range ~0-3.3V
    pinMode(ADC_CH1_PIN, INPUT);
    pinMode(ADC_CH2_PIN, INPUT);

    // Initialize double buffers
    for (int i = 0; i < 2; i++) {
        adc_buf[i].write_idx = 0;
        adc_buf[i].ready = false;
    }

    // Initialize hardware timer for 250 Hz sampling
    // Timer 0, prescaler 80 (1 MHz tick), count up
    adc_timer = timerBegin(0, 80, true);
    timerAttachInterrupt(adc_timer, &onAdcTimer, true);
    timerAlarmWrite(adc_timer, SAMPLE_INTERVAL_US, true);  // 4000 us = 250 Hz
    timerAlarmEnable(adc_timer);

    // Initialize signal processing
    signal_init();
    ui_set_signal_status(&ui, "ADC");

    // Pre-fill waveform buffer with flat line
    for (int i = 0; i < 2560; i++) {
        ui_add_sample_ch1(&ui, 0);
        ui_add_sample_ch2(&ui, 0);
    }

    Serial.println("ADC timer started at 250 Hz");
#elif USE_SIGNAL_PROCESSING == 1
    // Initialize signal processing with dummy input
    signal_init();
    ui_set_signal_status(&ui, "Signal");

    // Pre-fill waveform buffer with simple waveform for immediate display
    for (int i = 0; i < 2560; i++) {
        float t = i * (1.0f / SAMPLE_RATE_HZ);
        float ch1 = sinf(2 * 3.14159f * 10 * t) * 50;
        float ch2 = sinf(2 * 3.14159f * 8 * t) * 50;
        ui_add_sample_ch1(&ui, ch1);
        ui_add_sample_ch2(&ui, ch2);
    }
#else
    // Initialize synthetic data
    synthetic_init();
    ui_set_signal_status(&ui, "Synthetic");

    // Pre-fill waveform buffer with 10 seconds of synthetic data
    for (int i = 0; i < 2560; i++) {
        float ch1 = synthetic_get_ch1_sample();
        float ch2 = synthetic_get_ch2_sample();
        ui_add_sample_ch1(&ui, ch1);
        ui_add_sample_ch2(&ui, ch2);
    }
#endif

    send_cmd(SPI_DISPLAY_ENABLE);
    Serial.println("UI initialized");

    // Initialize timing to prevent catch-up burst on first loop
    last_sample_time = micros();
    last_fps_time = millis();
    }  // End of mode 'd'
}

// Dummy raw sample counter for signal processing mode
static uint32_t sample_counter = 0;

void loop() {
    unsigned long now = micros();

    // Sample at 250 Hz
#if USE_SIGNAL_PROCESSING == 3
    // Process ADS1299 DRDY interrupt flag
    ads1299_esp32s2_process();

    // Check if buffer is ready
    int32_t *ch1_buf, *ch2_buf;
    uint16_t count;
    if (ads1299_esp32s2_get_buffer(&ch1_buf, &ch2_buf, &count)) {
        for (int i = 0; i < count; i++) {
            float ch1 = signal_process(ch1_buf[i], 0);
            float ch2 = signal_process(ch2_buf[i], 1);
            ui_add_sample_ch1(&ui, ch1);
            ui_add_sample_ch2(&ui, ch2);
        }
    }
#elif USE_SIGNAL_PROCESSING == 2
    // Process any ready ADC buffer from timer ISR
    // Check both buffers for ready data
    for (int b = 0; b < 2; b++) {
        if (adc_buf[b].ready) {
            // Process all samples in this buffer
            for (int i = 0; i < ADC_BUFFER_SIZE; i++) {
                float ch1 = signal_process(adc_buf[b].ch1[i], 0);
                float ch2 = signal_process(adc_buf[b].ch2[i], 1);
                ui_add_sample_ch1(&ui, ch1);
                ui_add_sample_ch2(&ui, ch2);
            }
            adc_buf[b].ready = false;  // Mark as processed
        }
    }
#else
    // For synthetic/dummy: use catch-up to maintain correct timing
    while (now - last_sample_time >= SAMPLE_INTERVAL_US) {
        last_sample_time += SAMPLE_INTERVAL_US;

#if USE_SIGNAL_PROCESSING == 1
        // Generate dummy raw samples (simulating ADC input)
        float t = sample_counter * (1.0f / SAMPLE_RATE_HZ);
        int32_t raw_ch1 = (int32_t)(
            sin(2 * 3.14159f * 10 * t) * 500 +   // 10 Hz alpha
            sin(2 * 3.14159f * 20 * t) * 300 +   // 20 Hz beta
            sin(2 * 3.14159f * 5 * t) * 200      // 5 Hz theta
        );
        int32_t raw_ch2 = (int32_t)(
            sin(2 * 3.14159f * 8 * t) * 400 +    // 8 Hz alpha
            sin(2 * 3.14159f * 15 * t) * 350 +   // 15 Hz beta
            sin(2 * 3.14159f * 3 * t) * 250      // 3 Hz delta
        );
        sample_counter++;

        float ch1 = signal_process(raw_ch1, 0);
        float ch2 = signal_process(raw_ch2, 1);
#else
        float ch1 = synthetic_get_ch1_sample();
        float ch2 = synthetic_get_ch2_sample();
#endif
        ui_add_sample_ch1(&ui, ch1);
        ui_add_sample_ch2(&ui, ch2);
    }
#endif  // USE_SIGNAL_PROCESSING == 2

    // Update indices every 1 second
    if (millis() - last_index_update >= 1000) {
        last_index_update = millis();

#if USE_SIGNAL_PROCESSING >= 1
        // Check if new indices are available from signal processing
        if (signal_indices_updated()) {
            signal_indices_t sig_indices;
            signal_get_indices(&sig_indices);
            cognitive_indices_t indices = {
                .focus = sig_indices.focus,
                .relaxation = sig_indices.relaxation,
                .fatigue = sig_indices.fatigue,
                .meditation = sig_indices.meditation
            };
            ui_update_indices(&ui, &indices);
        }
#else
        cognitive_indices_t indices = {
            .focus = synthetic_get_index(0),
            .relaxation = synthetic_get_index(1),
            .fatigue = synthetic_get_index(2),
            .meditation = synthetic_get_index(3)
        };
        ui_update_indices(&ui, &indices);
#endif
    }

    // Render and flush UI
    unsigned long t0 = micros();
    ui_render(&ui);
    unsigned long t1 = micros();
    ui_flush(&ui);
    unsigned long t2 = micros();
    frame_count++;

    // Print FPS and timing every second
    if (millis() - last_fps_time >= 1000) {
        Serial.printf("FPS: %d  render: %lu us  flush: %lu us\n",
                      frame_count, t1 - t0, t2 - t1);
        frame_count = 0;
        last_fps_time = millis();
    }
}
