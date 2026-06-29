#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"

#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <driver/ledc.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#define TAG "CompactWifiBoardLcdServo"

/**
 * @brief Board: Bread Compact WiFi + LCD + 2D Servo (Pan/Tilt)
 *
 * Hardware:
 *   - ESP32-S3
 *   - ST7789 240x320 SPI LCD
 *   - MEMS I2S mic + I2S digital amplifier (NoAudioCodecSimplex)
 *   - Boot button, built-in LED, lamp controller (GPIO18)
 *   - 2-axis servo control via LEDC PWM (Pan on GPIO1, Tilt on GPIO2)
 */
class CompactWifiBoardLcdServo : public WifiBoard {
private:
    Button boot_button_;
    LcdDisplay* display_;

    // Lamp state
    bool lamp_on_;

    // Servo state
    int pan_angle_;
    int tilt_angle_;

    /**
     * @brief Initialize SPI bus for LCD display (SPI3_HOST)
     */
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    /**
     * @brief Initialize ST7789 LCD display
     */
    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 2;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        display_ = new SpiLcdDisplay(panel_io, panel,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                     DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    /**
     * @brief Initialize boot button
     */
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    /**
     * @brief Convert servo angle (0-90°) to LEDC duty value for 50Hz 14-bit PWM
     *
     * Standard servo timing:
     *   - Period = 20ms = 20000us
     *   - 14-bit resolution = 16384 steps
     *   - 0°  = SERVO_MIN_PULSE_US (500us) -> duty = 500 * 16384 / 20000 ≈ 410
     *   - 90° = SERVO_MAX_PULSE_US (2500us) -> duty = 2500 * 16384 / 20000 ≈ 2048
     *   - 45° = 1500us -> duty = 1500 * 16384 / 20000 ≈ 1229
     */
    static uint32_t AngleToDuty(int angle_deg) {
        if (angle_deg < 0) angle_deg = 0;
        if (angle_deg > 90) angle_deg = 90;
        int pulse_us = SERVO_MIN_PULSE_US +
                       (angle_deg * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) / 90;
        return (pulse_us * 16384) / 20000;
    }

    /**
     * @brief Initialize LEDC PWM for servo control
     *
     * Uses LEDC_TIMER_0 at 50Hz with 14-bit duty resolution.
     * Channel 0: Pan servo  (GPIO1)
     * Channel 1: Tilt servo (GPIO2)
     * Both servos default to center position (45°).
     */
    void InitializeServo() {
        ledc_timer_config_t ledc_timer = {};
        ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
        ledc_timer.duty_resolution = LEDC_TIMER_14_BIT;
        ledc_timer.timer_num = LEDC_TIMER_0;
        ledc_timer.freq_hz = SERVO_PWM_FREQ;
        ledc_timer.clk_cfg = LEDC_AUTO_CLK;
        ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

        // Pan servo channel
        ledc_channel_config_t pan_channel = {};
        pan_channel.gpio_num = SERVO_PAN_GPIO;
        pan_channel.speed_mode = LEDC_LOW_SPEED_MODE;
        pan_channel.channel = LEDC_CHANNEL_0;
        pan_channel.timer_sel = LEDC_TIMER_0;
        pan_channel.duty = AngleToDuty(45);
        pan_channel.hpoint = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&pan_channel));

        // Tilt servo channel
        ledc_channel_config_t tilt_channel = {};
        tilt_channel.gpio_num = SERVO_TILT_GPIO;
        tilt_channel.speed_mode = LEDC_LOW_SPEED_MODE;
        tilt_channel.channel = LEDC_CHANNEL_1;
        tilt_channel.timer_sel = LEDC_TIMER_0;
        tilt_channel.duty = AngleToDuty(45);
        tilt_channel.hpoint = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&tilt_channel));

        pan_angle_ = 45;
        tilt_angle_ = 45;

        ESP_LOGI(TAG, "Servo PWM initialized: Pan=GPIO%d, Tilt=GPIO%d, Freq=%dHz",
                 SERVO_PAN_GPIO, SERVO_TILT_GPIO, SERVO_PWM_FREQ);
    }

    /**
     * @brief Set pan servo to specified angle (clamped to 0-90)
     */
    void SetPanAngle(int angle) {
        if (angle < 0) angle = 0;
        if (angle > 90) angle = 90;
        pan_angle_ = angle;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, AngleToDuty(pan_angle_));
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }

    /**
     * @brief Set tilt servo to specified angle (clamped to 0-90)
     */
    void SetTiltAngle(int angle) {
        if (angle < 0) angle = 0;
        if (angle > 90) angle = 90;
        tilt_angle_ = angle;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, AngleToDuty(tilt_angle_));
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    }

    /**
     * @brief Register MCP tools for lamp and servo control
     */
    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();

        // Lamp controller (direct GPIO toggle)
        mcp_server.AddTool("self.lamp.toggle",
            "Turn the lamp on/off (GPIO18)",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                lamp_on_ = !lamp_on_;
                gpio_set_level((gpio_num_t)LAMP_GPIO, lamp_on_ ? 1 : 0);
                return lamp_on_;
            });

        // Pan servo control
        mcp_server.AddTool("self.servo.pan",
            "Set the horizontal (pan) servo angle from 0 to 90 degrees",
            PropertyList({
                Property("angle", kPropertyTypeInteger, 0, 90),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int angle = properties["angle"].value<int>();
                SetPanAngle(angle);
                char buf[64];
                snprintf(buf, sizeof(buf), "{\"pan\":%d}", pan_angle_);
                return std::string(buf);
            });

        // Tilt servo control
        mcp_server.AddTool("self.servo.tilt",
            "Set the vertical (tilt) servo angle from 0 to 90 degrees",
            PropertyList({
                Property("angle", kPropertyTypeInteger, 0, 90),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int angle = properties["angle"].value<int>();
                SetTiltAngle(angle);
                char buf[64];
                snprintf(buf, sizeof(buf), "{\"tilt\":%d}", tilt_angle_);
                return std::string(buf);
            });

        // Center both servos
        mcp_server.AddTool("self.servo.center",
            "Center both pan and tilt servos to 45 degrees",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                SetPanAngle(45);
                SetTiltAngle(45);
                return true;
            });
    }

public:
    CompactWifiBoardLcdServo()
        : boot_button_(BOOT_BUTTON_GPIO)
        , display_(nullptr)
        , lamp_on_(false)
        , pan_angle_(45)
        , tilt_angle_(45) {
        ESP_LOGI(TAG, "Initializing Bread Compact WiFi + LCD + Servo board");

        // Initialize lamp GPIO
        gpio_config_t lamp_cfg = {};
        lamp_cfg.pin_bit_mask = (1ULL << LAMP_GPIO);
        lamp_cfg.mode = GPIO_MODE_OUTPUT;
        lamp_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
        lamp_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        lamp_cfg.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&lamp_cfg);

        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeServo();
        InitializeTools();
        GetBacklight()->SetBrightness(100);
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_MIC_GPIO_SCK,
            AUDIO_I2S_MIC_GPIO_WS,
            AUDIO_I2S_SPK_GPIO_BCLK,
            AUDIO_I2S_SPK_GPIO_LRCK,
            AUDIO_I2S_MIC_GPIO_DIN,
            AUDIO_I2S_SPK_GPIO_DOUT);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(CompactWifiBoardLcdServo);
