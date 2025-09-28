#include "rm67162.h"
#include "SPI.h"
#include "Arduino.h"
#include "driver/spi_master.h"
#include "font5x7.h"

const static lcd_cmd_t rm67162_spi_init[] = {
    {0xFE, {0x00}, 0x01}, // PAGE
    {0x36, {0x00}, 0x01}, // Scan Direction Control
    {0x3A, {0x75}, 0x01}, // Interface Pixel Format 16bit/pixel
    {0x51, {0x00}, 0x01},        // Write Display Brightness MAX_VAL=0XFF
    {0x11, {0x00}, 0x01 | 0x80}, // Sleep Out
    {0x29, {0x00}, 0x01 | 0x80}, // Display on
    {0x51, {0xD0}, 0x01},        // Brightness
};

const static lcd_cmd_t rm67162_qspi_init[] = {
    {0x11, {0x00}, 0x80}, // Sleep Out
    {0x3A, {0x55}, 0x01}, // Interface Pixel Format 16bit/pixel
    {0x51, {0x00}, 0x01}, // Brightness
    {0x29, {0x00}, 0x80}, // Display on
    {0x51, {0xD0}, 0x01}, // Brightness
};

static spi_device_handle_t spi;

static void WriteComm(uint8_t data) {
    TFT_CS_L;
    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
    TFT_DC_L;
    SPI.write(data);
    TFT_DC_H;
    SPI.endTransaction();
    TFT_CS_H;
}

static void WriteData(uint8_t data) {
    TFT_CS_L;
    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
    TFT_DC_H;
    SPI.write(data);
    SPI.endTransaction();
    TFT_CS_H;
}

static void WriteData16(uint16_t data) {
    TFT_CS_L;
    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
    TFT_DC_H;
    SPI.write16(data);
    SPI.endTransaction();
    TFT_CS_H;
}

static void lcd_send_cmd(uint32_t cmd, uint8_t *dat, uint32_t len) {
#if LCD_USB_QSPI_DREVER == 1
    TFT_CS_L;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.flags = (SPI_TRANS_MULTILINE_CMD | SPI_TRANS_MULTILINE_ADDR);
    t.cmd = 0x02;
    t.addr = cmd << 8;
    if (len != 0) {
        t.tx_buffer = dat;
        t.length = 8 * len;
    } else {
        t.tx_buffer = NULL;
        t.length = 0;
    }
    spi_device_polling_transmit(spi, &t);
    TFT_CS_H;
#else
    WriteComm(cmd);
    if (len != 0) {
        for (int i = 0; i < len; i++)
            WriteData(dat[i]);
    }
#endif
}

void rm67162_init(void) {
    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_RES, OUTPUT);

    TFT_RES_L;
    delay(300);
    TFT_RES_H;
    delay(200);

#if LCD_USB_QSPI_DREVER == 1
    esp_err_t ret;

    spi_bus_config_t buscfg = {
        .data0_io_num = TFT_QSPI_D0,
        .data1_io_num = TFT_QSPI_D1,
        .sclk_io_num = TFT_QSPI_SCK,
        .data2_io_num = TFT_QSPI_D2,
        .data3_io_num = TFT_QSPI_D3,
        .max_transfer_sz = (SEND_BUF_SIZE * 16) + 8,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS,
    };
    spi_device_interface_config_t devcfg = {
        .command_bits = 8,
        .address_bits = 24,
        .mode = TFT_SPI_MODE,
        .clock_speed_hz = SPI_FREQUENCY,
        .spics_io_num = -1,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .queue_size = 17,
    };
    ret = spi_bus_initialize(TFT_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    ret = spi_bus_add_device(TFT_SPI_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);

#else
    SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
    SPI.setFrequency(SPI_FREQUENCY);
    pinMode(TFT_DC, OUTPUT);
#endif

#if LCD_USB_QSPI_DREVER == 1
    const lcd_cmd_t *lcd_init = rm67162_qspi_init;
    for (int i = 0; i < sizeof(rm67162_qspi_init) / sizeof(lcd_cmd_t); i++)
#else
    const lcd_cmd_t *lcd_init = rm67162_spi_init;
    for (int i = 0; i < sizeof(rm67162_spi_init) / sizeof(lcd_cmd_t); i++)
#endif
    {
        lcd_send_cmd(lcd_init[i].cmd,
                     (uint8_t *)lcd_init[i].data,
                     lcd_init[i].len & 0x7f);

        if (lcd_init[i].len & 0x80)
            delay(120);
    }
}

void lcd_setRotation(uint8_t r) {
    uint8_t gbr = TFT_MAD_RGB;

    switch (r) {
    case 1: gbr = TFT_MAD_MX | TFT_MAD_MV | gbr; break;
    case 2: gbr = TFT_MAD_MX | TFT_MAD_MY | gbr; break;
    case 3: gbr = TFT_MAD_MV | TFT_MAD_MY | gbr; break;
    }
    lcd_send_cmd(TFT_MADCTL, &gbr, 1);
}

void lcd_address_set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    lcd_cmd_t t[3] = {
        {0x2a, {x1 >> 8, x1, x2 >> 8, x2}, 0x04},
        {0x2b, {y1 >> 8, y1, y2 >> 8, y2}, 0x04},
        {0x2c, {0x00}, 0x00},
    };
    for (uint32_t i = 0; i < 3; i++) {
        lcd_send_cmd(t[i].cmd, t[i].data, t[i].len);
    }
}

void lcd_fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color) {
    uint16_t w = xend - xsta;
    uint16_t h = yend - ysta;
    uint16_t *color_p = (uint16_t *)heap_caps_malloc(w * h * 2, MALLOC_CAP_INTERNAL);
    for (int i = 0; i < w * h; i++) color_p[i] = color;
    lcd_PushColors(xsta, ysta, w, h, color_p);
    free(color_p);
}

void lcd_DrawPoint(uint16_t x, uint16_t y, uint16_t color) {
    lcd_address_set(x, y, x + 1, y + 1);
    lcd_PushColors(&color, 1);
}

void lcd_PushColors(uint16_t x, uint16_t y, uint16_t width, uint16_t high, uint16_t *data) {
#if LCD_USB_QSPI_DREVER == 1
    // (QSPI implementation here — already in your upload)
#else
    lcd_address_set(x, y, x + width - 1, y + high - 1);
    TFT_CS_L;
    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
    TFT_DC_H;
    SPI.writeBytes((uint8_t *)data, width * high * 2);
    SPI.endTransaction();
    TFT_CS_H;
#endif
}

void lcd_PushColors(uint16_t *data, uint32_t len) {
#if LCD_USB_QSPI_DREVER == 1
    // (QSPI implementation here — already in your upload)
#else
    TFT_CS_L;
    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
    TFT_DC_H;
    SPI.writeBytes((uint8_t *)data, len * 2);
    SPI.endTransaction();
    TFT_CS_H;
#endif
}

void lcd_sleep() { lcd_send_cmd(0x10, NULL, 0); }
void lcd_brightness(uint8_t bright) { lcd_send_cmd(0x51, &bright, 0x01); }
void lcd_set_colour_enhance(uint8_t enh) { lcd_send_cmd(0x58, &enh, 0x01); }
void lcd_display_off() { lcd_send_cmd(0x28, NULL, 0x01); }
void lcd_display_on() { lcd_send_cmd(0x29, NULL, 0x01); }
void lcd_display_invert_on() { lcd_send_cmd(0x21, NULL, 0x01); }
void lcd_display_invert_off() { lcd_send_cmd(0x20, NULL, 0x01); }
void lcd_display_set_colour_enhance_low_byte(uint8_t ce_low_byte) { lcd_send_cmd(0x5A, &ce_low_byte, 0x01); }
void lcd_display_set_colour_enhance_high_byte(uint8_t ce_high_byte) { lcd_send_cmd(0x5B, &ce_high_byte, 0x01); }
void lcd_display_high_brightness_mode_on(uint8_t hbm_en) { lcd_send_cmd(0xB0, &hbm_en, 0x01); }
void lcd_display_high_brightness_mode_off(uint8_t hbm_en) { lcd_send_cmd(0xB0, &hbm_en, 0x01); }

// ============================
// Text rendering functions
// ============================

void lcd_drawChar(int16_t x, int16_t y, char c,
                  uint16_t color, uint16_t bg, uint8_t size) {
    if (c < 32 || c > 126) return;
    const uint8_t *bitmap = font5x7[c - 32];

    for (int8_t i = 0; i < 5; i++) {
        uint8_t line = pgm_read_byte(&bitmap[i]);
        for (int8_t j = 0; j < 8; j++) {
            if (line & 0x1) {
                if (size == 1) lcd_DrawPoint(x + i, y + j, color);
                else lcd_fill(x + i*size, y + j*size,
                              x + (i+1)*size, y + (j+1)*size, color);
            } else if (bg != color) {
                if (size == 1) lcd_DrawPoint(x + i, y + j, bg);
                else lcd_fill(x + i*size, y + j*size,
                              x + (i+1)*size, y + (j+1)*size, bg);
            }
            line >>= 1;
        }
    }
}

void lcd_drawText(int16_t x, int16_t y, const char *text,
                  uint16_t color, uint16_t bg, uint8_t size) {
    while (*text) {
        lcd_drawChar(x, y, *text, color, bg, size);
        x += 6 * size;
        text++;
    }
}
