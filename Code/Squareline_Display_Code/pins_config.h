#pragma once

// Use QSPI or SPI
#define LCD_USB_QSPI_DREVER   1   // set 1 for QSPI, 0 for SPI

#define SPI_FREQUENCY         75000000
#define TFT_SPI_MODE          SPI_MODE0
#define TFT_SPI_HOST          SPI2_HOST

// Display resolution
#define TFT_WIDTH             240
#define TFT_HEIGHT            536
#define SEND_BUF_SIZE         (0x4000)

#define TFT_DC                7
#define TFT_RES               17
#define TFT_CS                6
#define TFT_MOSI              18
#define TFT_SCK               47

// QSPI pins
#define TFT_QSPI_CS           6
#define TFT_QSPI_SCK          47
#define TFT_QSPI_D0           18
#define TFT_QSPI_D1           7
#define TFT_QSPI_D2           48
#define TFT_QSPI_D3           5

// Backlight & misc
#define PIN_LED               38
#define PIN_BAT_VOLT          4
#define PIN_BUTTON_1          0
#define PIN_BUTTON_2          21

// SD card (if used)
#define PIN_SD_CMD    13
#define PIN_SD_CLK    11
#define PIN_SD_D0     12
