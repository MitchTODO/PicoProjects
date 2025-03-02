#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define SPI_PORT spi0
#define PIN_SCK  2
#define PIN_MOSI 3
#define PIN_MISO 4
#define PIN_CS   5
#define PIN_DC   6
#define PIN_RST  7
#define WIDTH 480
#define HEIGHT 320

// Data will be copied from src to dst
const char src[] = "Hello, world! (from DMA)";
char dst[count_of(src)];
uint8_t data[4];
uint8_t r,g,b;

void lcd_command(uint8_t cmd) {
    gpio_put(PIN_DC, 1);
    spi_write_blocking(SPI_PORT, (uint8_t[]){cmd}, 1);
}

void lcd_data(uint8_t data[], size_t len) {
    gpio_put(PIN_DC, 0);
    spi_write_blocking(SPI_PORT, data, len);
}

void lcd_call(uint8_t cmd, uint8_t data[], size_t len) {
    lcd_command(cmd);
    lcd_data(data, len);
}

void lcd_reset() {
    gpio_put(PIN_RST, 0);
    sleep_ms(120);
    gpio_put(PIN_RST, 1);
    sleep_ms(120);
}

void lcd_init() {
    lcd_reset();
    lcd_command(0x11); // Sleep out
    sleep_ms(120);
    lcd_call(0x36, (uint8_t[]){0x28}, 1); // Memory Access Control top-down BGR order
    lcd_call(0x3A, (uint8_t[]){0x07}, 1); // Set to 24-bit color mode (RGB888)
    lcd_command(0x21); // invert colors
    sleep_ms(120);
    lcd_command(0x29); // Display ON
}

void lcd_draw() {
    lcd_command(0x2c);
    gpio_put(PIN_DC, 1); // Data mode
}

void lcd_set_range(int x, int y, int w, int h) {
    w -= 1;
    h -= 1;
    lcd_call(0x2a, (uint8_t[]){x >> 8 & 0xff, x & 0xff, x + w >> 8 & 0xff, x + w & 0xff}, 4);
    lcd_call(0x2b, (uint8_t[]){y >> 8 & 0xff, y & 0xff, y + h >> 8 & 0xff, y + h & 0xff}, 4);
}

void lcd_set_color(uint8_t _r, uint8_t _g, uint8_t _b) {
    r = _r;
    g = _g;
    b = _b;
}

void lcd_fill(int x, int y, int w, int h) {
    lcd_set_range(x, y, w, h);
    lcd_draw();
    size_t w3 = w * 3;
    uint8_t row[w3];
    for (size_t i=0; i < w3; i += 3) {
        row[i] = r;
        row[i+1] = g;
        row[i+2] = b;
    }
    for (size_t i = 0; i < y; i++) {
        gpio_put(PIN_DC, 0);
        spi_write_blocking(SPI_PORT, row, w3);
    }
}

void lcd_clear(uint8_t r, uint8_t g, uint8_t b) {
    lcd_set_range(0, 0, WIDTH, HEIGHT);
    lcd_draw();
    size_t w3 = WIDTH * 3;
    uint8_t row[w3];
    for (size_t i=0; i < w3; i += 3) {
        row[i] = r;
        row[i+1] = g;
        row[i+2] = b;
    }
    for (int i=0; i < HEIGHT; i++) {
        spi_write_blocking(SPI_PORT, row, w3);
    }
}

int main() {
    stdio_init_all();

    // SPI initialisation. This example will use SPI at 1MHz.
    spi_init(SPI_PORT, 1000*1000); // TODO: change to -1 for max clock
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_DC,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_RST,   GPIO_FUNC_SIO);
    
    // Chip select is active-low, so we'll initialise it to a driven-high state
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_set_dir(PIN_RST, GPIO_OUT);
    gpio_pull_down(PIN_CS);
    gpio_put(PIN_CS, 0);
    // For more examples of SPI use see https://github.com/raspberrypi/pico-examples/tree/master/spi

    // Get a free channel, panic() if there are none
     int chan = dma_claim_unused_channel(true);
    
    // 8 bit transfers. Both read and write address increment after each
    // transfer (each pointing to a location in src or dst respectively).
    // No DREQ is selected, so the DMA transfers as fast as it can.
    
    dma_channel_config c = dma_channel_get_default_config(chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, true);
    
    dma_channel_configure(
        chan,          // Channel to be configured
        &c,            // The configuration we just created
        dst,           // The initial write address
        src,           // The initial read address
        count_of(src), // Number of transfers; in this case each is 1 byte.
        true           // Start immediately.
    );
    
    // We could choose to go and do something else whilst the DMA is doing its
    // thing. In this case the processor has nothing else to do, so we just
    // wait for the DMA to finish.
    dma_channel_wait_for_finish_blocking(chan);
    
    // The DMA has now copied our text from the transmit buffer (src) to the
    // receive buffer (dst), so we can print it out from there.
    puts(dst);

    lcd_init();
    lcd_clear(0, 154, 0);
    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
