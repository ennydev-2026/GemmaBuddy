#include <stdio.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "application.h"
#include "assets/lang_config.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "wifi_board.h"
#include "board_power_bsp.h"
#include "custom_lcd_display.h"
#include "lvgl.h"
#include "mcp_server.h"

#define TAG "waveshare_epaper_1_54"

namespace {

constexpr uint8_t kShtc3Address = 0x70;
constexpr uint16_t kShtc3WakeCommand = 0x3517;
constexpr uint16_t kShtc3SleepCommand = 0xB098;
constexpr uint16_t kShtc3MeasureCommand = 0x7CA2;
constexpr uint16_t kShtc3SoftResetCommand = 0x805D;

class Shtc3Sensor {
  public:
    bool Init(i2c_master_bus_handle_t bus) {
        bus_ = bus;
        return EnsureDevice() && WakeAndSleep();
    }

    bool Read(float &temperature_c, float &humidity_percent) {
        if (!EnsureDevice()) {
            return false;
        }

        if (!WakeAndSleep()) {
            ResetDevice();
            if (!WakeAndSleep()) {
                return false;
            }
        }

        if (!SendCommand(kShtc3WakeCommand)) {
            return false;
        }
        if (!SendCommand(kShtc3MeasureCommand)) {
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(20));

        uint8_t data[6] = {};
        esp_err_t err = i2c_master_receive(dev_, data, sizeof(data), 100);
        SendCommand(kShtc3SleepCommand);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "SHTC3 read failed: %s", esp_err_to_name(err));
            return false;
        }

        if (Crc8(data, 2) != data[2] || Crc8(data + 3, 2) != data[5]) {
            ESP_LOGW(TAG, "SHTC3 CRC mismatch");
            return false;
        }

        uint16_t raw_temp = (static_cast<uint16_t>(data[0]) << 8) | data[1];
        uint16_t raw_humidity = (static_cast<uint16_t>(data[3]) << 8) | data[4];

        temperature_c = -45.0f + 175.0f * (static_cast<float>(raw_temp) / 65535.0f);
        humidity_percent = 100.0f * (static_cast<float>(raw_humidity) / 65535.0f);
        humidity_percent = std::clamp(humidity_percent, 0.0f, 100.0f);
        return true;
    }

  private:
    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t dev_ = nullptr;
    bool device_added_ = false;

    bool EnsureDevice() {
        if (device_added_) {
            return true;
        }

        i2c_device_config_t cfg = {
            .device_address = kShtc3Address,
            .scl_speed_hz = 100000,
            .scl_wait_us = 0,
        };
        if (i2c_master_bus_add_device(bus_, &cfg, &dev_) != ESP_OK) {
            ESP_LOGW(TAG, "SHTC3 device add failed");
            return false;
        }
        device_added_ = true;

        esp_err_t probe = i2c_master_probe(bus_, kShtc3Address, 100);
        if (probe != ESP_OK) {
            ESP_LOGW(TAG, "SHTC3 probe failed: %s", esp_err_to_name(probe));
            return false;
        }
        return true;
    }

    bool WakeAndSleep() {
        if (!SendCommand(kShtc3WakeCommand)) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
        return SendCommand(kShtc3SleepCommand);
    }

    void ResetDevice() {
        SendCommand(kShtc3SoftResetCommand);
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    bool SendCommand(uint16_t command) {
        uint8_t payload[2] = {
            static_cast<uint8_t>(command >> 8),
            static_cast<uint8_t>(command & 0xFF),
        };
        return i2c_master_transmit(dev_, payload, sizeof(payload), 100) == ESP_OK;
    }

    static uint8_t Crc8(const uint8_t *data, size_t len) {
        uint8_t crc = 0xFF;
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int bit = 0; bit < 8; ++bit) {
                crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31) : static_cast<uint8_t>(crc << 1);
            }
        }
        return crc;
    }
};

}  // namespace

class CustomBoard : public WifiBoard {
  private:
    i2c_master_bus_handle_t   i2c_bus_;
    Button                    boot_button_;
    Button                    pwr_button_;
    CustomLcdDisplay         *display_;
    BoardPowerBsp            *power_;
    Shtc3Sensor               shtc3_;
    adc_oneshot_unit_handle_t adc1_handle;
    adc_cali_handle_t         cali_handle;
    float                     last_temperature_c_ = NAN;
    float                     last_humidity_percent_ = NAN;
    bool                      climate_valid_ = false;

    static void ClimateTask(void *arg) {
        auto *self = static_cast<CustomBoard *>(arg);

        while (true) {
            self->UpdateClimateReading();
            vTaskDelay(pdMS_TO_TICKS(15000));
        }
    }

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {};
        i2c_bus_cfg.i2c_port          = (i2c_port_t) 0;
        i2c_bus_cfg.sda_io_num        = AUDIO_CODEC_I2C_SDA_PIN;
        i2c_bus_cfg.scl_io_num        = AUDIO_CODEC_I2C_SCL_PIN;
        i2c_bus_cfg.clk_source        = I2C_CLK_SRC_DEFAULT;
        i2c_bus_cfg.glitch_ignore_cnt = 7;
        i2c_bus_cfg.intr_priority     = 0;
        i2c_bus_cfg.trans_queue_depth = 0;
        i2c_bus_cfg.flags.enable_internal_pullup = 1;
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto &app = Application::GetInstance();
            // During startup (before connected), pressing BOOT button enters Wi-Fi config mode without reboot
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            if (app.GetDeviceState() == kDeviceStateListening || app.GetDeviceState() == kDeviceStateSpeaking) {
                display_->ShowCancelledFeedback();
                app.PlaySound(Lang::Sounds::OGG_VIBRATION);
            }
            app.ToggleChatState();
        });

        pwr_button_.OnLongPress([this]() {
            GetDisplay()->SetChatMessage("system", "OFF");
            vTaskDelay(pdMS_TO_TICKS(1000));
            power_->PowerAudioOff();
            power_->PowerEpdOff();
            power_->VbatPowerOff();
        });
    }

    void InitializeTools() {
        auto &mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.disp.network", "重新配网", PropertyList(), [this](const PropertyList &) -> ReturnValue {
            EnterWifiConfigMode();
            return true;
        });
    }

    void InitializeLcdDisplay() {
        custom_lcd_spi_t lcd_spi_data = {};
        lcd_spi_data.cs               = EPD_CS_PIN;
        lcd_spi_data.dc               = EPD_DC_PIN;
        lcd_spi_data.rst              = EPD_RST_PIN;
        lcd_spi_data.busy             = EPD_BUSY_PIN;
        lcd_spi_data.mosi             = EPD_MOSI_PIN;
        lcd_spi_data.scl              = EPD_SCK_PIN;
        lcd_spi_data.spi_host         = EPD_SPI_NUM;
        lcd_spi_data.buffer_len       = 5000;
        display_                      = new CustomLcdDisplay(NULL, NULL, EXAMPLE_LCD_WIDTH, EXAMPLE_LCD_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY, lcd_spi_data);
    }

    void InitializeSensors() {
        shtc3_.Init(i2c_bus_);
        UpdateClimateReading();
        xTaskCreatePinnedToCore(ClimateTask, "waveshare_climate", 4096, this, 1, nullptr, 0);
    }

    void UpdateClimateReading() {
        float temperature_c = NAN;
        float humidity_percent = NAN;
        bool valid = shtc3_.Read(temperature_c, humidity_percent);

        if (!valid) {
            if (!climate_valid_) {
                display_->SetClimate(NAN, NAN, false);
            }
            return;
        }

        bool changed = !climate_valid_
            || std::isnan(last_temperature_c_)
            || std::isnan(last_humidity_percent_)
            || fabsf(last_temperature_c_ - temperature_c) >= 0.2f
            || fabsf(last_humidity_percent_ - humidity_percent) >= 1.0f;

        climate_valid_ = true;
        last_temperature_c_ = temperature_c;
        last_humidity_percent_ = humidity_percent;

        if (changed) {
            display_->SetClimate(temperature_c, humidity_percent, true);
        }
    }

    void Power_Init() {
        power_ = new BoardPowerBsp(EPD_PWR_PIN, Audio_PWR_PIN, VBAT_PWR_PIN);
        power_->VbatPowerOn();
        power_->PowerAudioOn();
        power_->PowerEpdOn();
        do {
            vTaskDelay(pdMS_TO_TICKS(10));
        } while (!gpio_get_level(VBAT_PWR_GPIO));
    }

    uint16_t BatterygetVoltage(void) {
        static bool initialized = false;
        static adc_oneshot_unit_handle_t adc_handle;
        static adc_cali_handle_t cali_handle = NULL;
        if (!initialized) {
            adc_oneshot_unit_init_cfg_t init_config = {
                .unit_id = ADC_UNIT_1,
            };
            adc_oneshot_new_unit(&init_config, &adc_handle);
    
            adc_oneshot_chan_cfg_t ch_config = {
                .atten = ADC_ATTEN_DB_12,
                .bitwidth = ADC_BITWIDTH_12,
            };
            adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_3, &ch_config);
    
            adc_cali_curve_fitting_config_t cali_config = {
                .unit_id = ADC_UNIT_1,
                .atten = ADC_ATTEN_DB_12,
                .bitwidth = ADC_BITWIDTH_12,
            };
            if (adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle) == ESP_OK) {
                initialized = true;
            }
        }

        if (initialized) {
            int raw_value = 0;
            int raw_voltage = 0;
            int voltage = 0; // mV
            adc_oneshot_read(adc_handle, ADC_CHANNEL_3, &raw_value);
            adc_cali_raw_to_voltage(cali_handle, raw_value, &raw_voltage);
            voltage =  raw_voltage * 2;
            // ESP_LOGI(TAG, "voltage: %dmV", voltage);
            return (uint16_t)voltage;
        }

        return 0;
    }

    uint8_t BatterygetPercent() {
        int voltage = 0;
        for (uint8_t i = 0; i < 10; i++) {
            voltage += BatterygetVoltage();
        }

        voltage /= 10;
        int percent = (-1 * voltage * voltage + 9016 * voltage - 19189000) / 10000;
        percent = (percent > 100) ? 100 : (percent < 0) ? 0 : percent;
        // ESP_LOGI(TAG, "voltage: %dmV, percentage: %d%%", voltage, percent);
        return (uint8_t)percent;
    }

  public:
    CustomBoard() : boot_button_(BOOT_BUTTON_GPIO), pwr_button_(VBAT_PWR_GPIO) {
        Power_Init();
        InitializeI2c();
        InitializeButtons();
        InitializeTools();
        InitializeLcdDisplay();
        InitializeSensors();
    }

    virtual AudioCodec *GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override {
        return display_;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        charging = false;
        discharging = !charging;
        level = (int)BatterygetPercent();

        return true;
    }
};

DECLARE_BOARD(CustomBoard);
