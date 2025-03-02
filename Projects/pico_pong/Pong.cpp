
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include <cmath>
#include <vector>

using namespace std;

// SPI Defines
#define SPI_PORT spi0
#define PIN_MISO 4
#define PIN_CS   5
#define PIN_SCK  2
#define PIN_MOSI 3
#define PIN_DC    6
#define PIN_RST   7
#define PIN_BUTTON_A  15  // Move paddle up
#define PIN_BUTTON_B  14  // Move paddle down

// Screen Dimensions
#define WIDTH 480
#define HEIGHT 320

// Pixel depth and colors
#define PIXEL_DEPTH 3
#define BACKGROUND_COLOR 0

// Paddle and Ball properties
#define PADDLE_WIDTH 10
#define PADDLE_HEIGHT 60
#define BALL_SIZE 10

// Speeds
#define PADDLE_SPEED 5
#define BALL_SPEED_X 3
#define BALL_SPEED_Y 3

#define JOY_X_PIN 26 // Not strictly needed if you only move vertically
#define JOY_Y_PIN 27

uint8_t clear_row[WIDTH * PIXEL_DEPTH];


// Forward declarations
void lcd_command(uint8_t cmd);
void lcd_data(uint8_t *data, size_t len);
void lcd_call(uint8_t cmd, uint8_t *data, size_t len);
void lcd_init();
void lcd_set_range(int x, int y, size_t w, size_t h);
void lcd_draw();
void lcd_clear(uint8_t r, uint8_t g, uint8_t b);

// Convert a point from "cartesian style" centered coordinates to screen coordinates
vector<float> convertToScreenCoords(vector<float> point, float width, float height) {
    if (point.size() != 2) return {};
    vector<float> result;
    float screenCenterX = width / 2.0;
    float screenCenterY = height / 2.0;
    result.push_back(point[0] + screenCenterX);
    result.push_back(-point[1] + screenCenterY);
    return result;
}


void lcd_command(uint8_t cmd) {
    gpio_put(PIN_DC, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
}

void lcd_data(uint8_t *data, size_t len) {
    gpio_put(PIN_DC, 1);
    spi_write_blocking(SPI_PORT, data, len);
}

void lcd_call(uint8_t cmd, uint8_t *data, size_t len) {
    lcd_command(cmd);
    lcd_data(data, len);
}

void lcd_init() {
    gpio_put(PIN_CS, 0);
    lcd_command(0x01); // Soft reset
    sleep_ms(120);
    lcd_command(0x11); // Sleep out
    uint8_t params[1];
    params[0] = 0x28;
    lcd_call(0x36, params, 1); // Memory access control
    //gpio_put(LSD,)
    params[0] = 0x07;
    lcd_call(0x3A, params, 1); // Interface pixel format (24-bit)
    lcd_command(0x21); // Invert colors
    lcd_command(0x29); // Display on
}

void lcd_draw() {
    lcd_command(0x2C);
    gpio_put(PIN_DC, 1);
}

void lcd_set_range(int x, int y, size_t w, size_t h) {
    w -= 1;
    h -= 1;
    uint8_t params[4];
    params[0] = (x >> 8) & 0xFF;
    params[1] = x & 0xFF;
    params[2] = ((x + w) >> 8) & 0xFF;
    params[3] = (x + w) & 0xFF;
    lcd_call(0x2A, params, 4);

    params[0] = (y >> 8) & 0xFF;
    params[1] = y & 0xFF;
    params[2] = ((y + h) >> 8) & 0xFF;
    params[3] = (y + h) & 0xFF;
    lcd_call(0x2B, params, 4);
}

void lcd_clear(uint8_t r, uint8_t g, uint8_t b) {
    lcd_set_range(0, 0, WIDTH, HEIGHT);
    lcd_draw();

    for (int i = 0; i < WIDTH; i++) {
        clear_row[i * 3] = r;
        clear_row[i * 3 + 1] = g;
        clear_row[i * 3 + 2] = b;
    }

    for (int i = 0; i < HEIGHT; i++) {
        spi_write_blocking(SPI_PORT, clear_row, WIDTH * PIXEL_DEPTH);
    }
}

// A helper to draw a solid rectangle of a given color
void drawRect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x + w > WIDTH || y + h > HEIGHT) return;
    lcd_set_range(x, y, w, h);
    lcd_draw();
    vector<uint8_t> buffer(w * h * PIXEL_DEPTH);
    for (int i = 0; i < w * h; i++) {
        buffer[i*3 + 0] = r;
        buffer[i*3 + 1] = g;
        buffer[i*3 + 2] = b;
    }
    spi_write_blocking(SPI_PORT, buffer.data(), buffer.size());
}

int main() {
    stdio_init_all();

    spi_init(SPI_PORT, -1); // Set SPI clock speed to 10 MHz
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS, GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_DC, GPIO_FUNC_SIO);
    gpio_set_function(PIN_RST, GPIO_FUNC_SIO);


    gpio_set_function(PIN_BUTTON_A, GPIO_FUNC_SIO);
    gpio_set_function(PIN_BUTTON_B, GPIO_FUNC_SIO);

    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_put(PIN_DC, 1);
    gpio_set_dir(PIN_RST, GPIO_OUT);
    gpio_put(PIN_RST, 1);
    gpio_set_dir(PIN_BUTTON_A, GPIO_IN);
    gpio_set_dir(PIN_BUTTON_B, GPIO_IN);

    lcd_init();
    lcd_clear(BACKGROUND_COLOR, BACKGROUND_COLOR, BACKGROUND_COLOR);
    
    // Initialize ADC
    adc_init();

    // Prepare the GPIO pins for ADC
    adc_gpio_init(JOY_X_PIN); // GPIO 26 is ADC0
    adc_gpio_init(JOY_Y_PIN); // GPIO 27 is ADC1
    sleep_ms(120);

    // Game variables
    int paddle_x = 10;
    int paddle_y = HEIGHT/2 - PADDLE_HEIGHT/2;

    int paddle2_x = WIDTH - 20;
    int paddle2_y = HEIGHT/2 - PADDLE_HEIGHT/2;

    int ball_x = WIDTH/2 - BALL_SIZE/2;
    int ball_y = HEIGHT/2 - BALL_SIZE/2;
    int ball_dx = BALL_SPEED_X;
    int ball_dy = BALL_SPEED_Y;

    // A simple "AI" for the second paddle: it will just follow the ball
    // vertically in a simplistic manner
    bool game_running = true;

        // Before main loop
    int old_paddle_y = paddle_y;
    int old_paddle2_y = paddle2_y;
    int old_ball_x = ball_x;
    int old_ball_y = ball_y;

    while (game_running) {
        // Store old positions
        old_paddle_y = paddle_y;
        old_paddle2_y = paddle2_y;
        old_ball_x = ball_x;
        old_ball_y = ball_y;

        // --- INPUT & LOGIC FIRST ---
        // Read joystick
        adc_select_input(1); // ADC1 for Y axis
        uint16_t y_raw = adc_read();
        int y_val = (int)y_raw - 2048;
        y_val = -y_val;
        int dead_zone = 400;

        // Move paddle based on joystick
        if (y_val > dead_zone && paddle_y < HEIGHT - PADDLE_HEIGHT) {
            paddle_y += PADDLE_SPEED;
        } else if (y_val < -dead_zone && paddle_y > 0) {
            paddle_y -= PADDLE_SPEED;
        }

        // AI paddle logic
        if (ball_y < paddle2_y + PADDLE_HEIGHT/2 && paddle2_y > 0) {
            paddle2_y -= PADDLE_SPEED;
        } else if (ball_y > paddle2_y + PADDLE_HEIGHT/2 && paddle2_y < HEIGHT - PADDLE_HEIGHT) {
            paddle2_y += PADDLE_SPEED;
        }

        // Move the ball
        ball_x += ball_dx;
        ball_y += ball_dy;

        // Check collisions
        if (ball_y <= 0 || ball_y + BALL_SIZE >= HEIGHT) {
            ball_dy = -ball_dy;
        }
        if (ball_x <= paddle_x + PADDLE_WIDTH && 
            ball_y + BALL_SIZE >= paddle_y && 
            ball_y <= paddle_y + PADDLE_HEIGHT) {
            ball_dx = -ball_dx;
            ball_x = paddle_x + PADDLE_WIDTH;
        }
        if (ball_x + BALL_SIZE >= paddle2_x && 
            ball_y + BALL_SIZE >= paddle2_y && 
            ball_y <= paddle2_y + PADDLE_HEIGHT) {
            ball_dx = -ball_dx;
            ball_x = paddle2_x - BALL_SIZE;
        }

        // Check scoring
        if (ball_x < 0) {
            // Reset ball
            ball_x = WIDTH/2 - BALL_SIZE/2;
            ball_y = HEIGHT/2 - BALL_SIZE/2;
            ball_dx = BALL_SPEED_X;
            ball_dy = BALL_SPEED_Y;
        } else if (ball_x + BALL_SIZE > WIDTH) {
            // Reset ball
            ball_x = WIDTH/2 - BALL_SIZE/2;
            ball_y = HEIGHT/2 - BALL_SIZE/2;
            ball_dx = BALL_SPEED_X;
            ball_dy = BALL_SPEED_Y;
        }

        // --- NOW DO ALL DISPLAY UPDATES TOGETHER ---
        // Erase old positions by drawing background color
        drawRect(paddle_x, old_paddle_y, PADDLE_WIDTH, PADDLE_HEIGHT, 0, 0, 0);
        drawRect(paddle2_x, old_paddle2_y, PADDLE_WIDTH, PADDLE_HEIGHT, 0, 0, 0);
        drawRect(old_ball_x, old_ball_y, BALL_SIZE, BALL_SIZE, 0, 0, 0);

        // Draw new positions
        drawRect(paddle_x, paddle_y, PADDLE_WIDTH, PADDLE_HEIGHT, 255, 255, 255);
        drawRect(paddle2_x, paddle2_y, PADDLE_WIDTH, PADDLE_HEIGHT, 255, 255, 255);
        drawRect(ball_x, ball_y, BALL_SIZE, BALL_SIZE, 255, 255, 255);

        sleep_ms(30);
        }
}
