#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// ==================== Audio Configuration ====================
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// I2S Microphone pins (Simplex mode)
#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6

// I2S Speaker pins (Simplex mode)
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

// Simplex mode: use separate I2S interfaces for mic and speaker
#define AUDIO_I2S_METHOD_SIMPLEX

// ==================== Button Configuration ====================
#define BOOT_BUTTON_GPIO GPIO_NUM_0

// ==================== LED Configuration ====================
#define BUILTIN_LED_GPIO GPIO_NUM_48

// ==================== Lamp Configuration ====================
#define LAMP_GPIO GPIO_NUM_18

// ==================== Display Configuration ====================
#define DISPLAY_SPI_SCK_PIN  GPIO_NUM_21
#define DISPLAY_SPI_MOSI_PIN GPIO_NUM_47
#define DISPLAY_DC_PIN       GPIO_NUM_40
#define DISPLAY_SPI_CS_PIN   GPIO_NUM_41
#define DISPLAY_RST_PIN      GPIO_NUM_45

#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY  false

#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

#define DISPLAY_BACKLIGHT_PIN          GPIO_NUM_42
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// ==================== Servo Configuration ====================
// Pan (horizontal) servo
#define SERVO_PAN_GPIO GPIO_NUM_1

// Tilt (vertical) servo
#define SERVO_TILT_GPIO GPIO_NUM_2

// Servo PWM parameters (50Hz standard servo)
#define SERVO_PWM_FREQ        50
#define SERVO_MIN_PULSE_US    500
#define SERVO_MAX_PULSE_US    2500

#endif // _BOARD_CONFIG_H_
