#include <stdio.h>
#include <math.h>
#include <esp_lcd_panel_io.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <vector>
#include <esp_log.h>
#include "custom_lcd_display.h"
#include "application.h"
#include "assets/lang_config.h"
#include "board.h"
#include "config.h"
#include "lvgl_theme.h"
#include "esp_lvgl_port.h"
#include "settings.h"
#include <font_awesome.h>

#define TAG "CustomLcdDisplay"

#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
#define BUFF_SIZE (EXAMPLE_LCD_WIDTH * EXAMPLE_LCD_HEIGHT * BYTES_PER_PIXEL)

const uint8_t WF_Full_1IN54[159] =
{											
    0x80,0x48,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x40,0x48,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x80,0x48,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x40,0x48,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0xA,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x8,0x1,0x0,0x8,0x1,0x0,0x2,				
    0xA,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,				
    0x22,0x22,0x22,0x22,0x22,0x22,0x0,0x0,0x0,			
    0x22,0x17,0x41,0x0,0x32,0x20
};

const uint8_t WF_PARTIAL_1IN54_0[159] =
{
    0x0,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x80,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x40,0x40,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x80,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0xF,0x0,0x0,0x0,0x0,0x0,0x0,
    0x1,0x1,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x0,0x0,0x0,0x0,0x0,0x0,0x0,
    0x22,0x22,0x22,0x22,0x22,0x22,0x0,0x0,0x0,
    0x02,0x17,0x41,0xB0,0x32,0x28,
};

void CustomLcdDisplay::lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p) {
    assert(disp != NULL);
    CustomLcdDisplay *driver = (CustomLcdDisplay *) lv_display_get_user_data(disp);
    uint16_t         *buffer = (uint16_t *) color_p;
    driver->EPD_Clear();
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            uint8_t color = (*buffer < 0x7fff) ? DRIVER_COLOR_BLACK : DRIVER_COLOR_WHITE;
            driver->EPD_DrawColorPixel(x, y, color);
            buffer++;
        }
    }
    driver->EPD_DisplayPart();
    lv_disp_flush_ready(disp);
}

CustomLcdDisplay::CustomLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, 
    int width, int height, int offset_x, int offset_y, 
    bool mirror_x, bool mirror_y, bool swap_xy, custom_lcd_spi_t _lcd_spi_data) : 
    LcdDisplay(panel_io, panel, width, height), 
    lcd_spi_data(_lcd_spi_data), 
    Width(width), Height(height) {

    ESP_LOGI(TAG, "Initialize SPI");
    spi_port_init();
    spi_gpio_init();

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority   = 2;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);
    lvgl_port_lock(0);

    buffer = (uint8_t *) heap_caps_malloc(lcd_spi_data.buffer_len, MALLOC_CAP_SPIRAM);
    assert(buffer);
    display_ = lv_display_create(width, height); /* 以水平和垂直分辨率（像素）进行基本初始化 */
    lv_display_set_flush_cb(display_, lvgl_flush_cb);
    lv_display_set_user_data(display_, this);

    uint8_t *buffer_1 = NULL;
    buffer_1          = (uint8_t *) heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
    assert(buffer_1);
    lv_display_set_buffers(display_, buffer_1, NULL, BUFF_SIZE, LV_DISPLAY_RENDER_MODE_FULL);

    ESP_LOGI(TAG, "EPD init");
    EPD_Init();
    EPD_Clear();
    EPD_Display();
    EPD_DisplayPartBaseImage();
    EPD_Init_Partial(); // 局部刷新初始化

    lvgl_port_unlock();
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    // Note: SetupUI() should be called by Application::Initialize(), not in constructor
    // to ensure lvgl objects are created after the display is fully initialized.
}

CustomLcdDisplay::~CustomLcdDisplay() {
    
}

void CustomLcdDisplay::SetupUI() {
    LcdDisplay::SetupUI();

    if (climate_label_ != nullptr) {
        return;
    }

    DisplayLockGuard lock(this);
    auto *lvgl_theme = static_cast<LvglTheme *>(current_theme_);
    auto *screen = lv_screen_active();
    auto *large_icon_font = lvgl_theme->large_icon_font()->font();

    lv_obj_align(emoji_box_, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_flex_flow(emoji_box_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(emoji_box_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(emoji_box_, lvgl_theme->spacing(3), 0);

    ai_icon_label_ = lv_label_create(emoji_box_);
    lv_obj_set_style_text_font(ai_icon_label_, large_icon_font, 0);
    lv_obj_set_style_text_color(ai_icon_label_, lvgl_theme->text_color(), 0);
    lv_label_set_text(ai_icon_label_, FONT_AWESOME_MICROCHIP_AI);

    battery_percent_overlay_ = lv_label_create(screen);
    lv_label_set_text(battery_percent_overlay_, "");
    lv_obj_set_style_text_color(battery_percent_overlay_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_text_font(battery_percent_overlay_, lvgl_theme->text_font()->font(), 0);
    lv_obj_set_style_text_align(battery_percent_overlay_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(battery_percent_overlay_, battery_label_, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

    feedback_label_ = lv_label_create(screen);
    lv_label_set_text(feedback_label_, "");
    lv_obj_set_style_text_color(feedback_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_text_align(feedback_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(feedback_label_, emoji_box_, LV_ALIGN_OUT_BOTTOM_MID, 0, lvgl_theme->spacing(2));

    climate_label_ = lv_label_create(screen);
    lv_label_set_text(climate_label_, "Reading climate...");
    lv_obj_set_style_text_color(climate_label_, lvgl_theme->text_color(), 0);
    lv_obj_set_style_text_align(climate_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(climate_label_, feedback_label_, LV_ALIGN_OUT_BOTTOM_MID, 0, lvgl_theme->spacing(2));

    char text[32];
    if (!climate_valid_ || !isfinite(cached_temperature_c_) || !isfinite(cached_humidity_percent_)) {
        snprintf(text, sizeof(text), "Climate unavailable");
    } else {
        snprintf(text, sizeof(text), "%.1fC  %.0f%%RH", cached_temperature_c_, cached_humidity_percent_);
    }
    lv_label_set_text(climate_label_, text);
    RefreshFeedbackLabel();
    UpdateOverlayVisibility(true);
}

void CustomLcdDisplay::SetStatus(const char* status) {
    if (HasActiveTransientFeedback() && strcmp(status, Lang::Strings::STANDBY) == 0) {
        return;
    }

    LcdDisplay::SetStatus(status);

    if (strcmp(status, Lang::Strings::LISTENING) == 0) {
        SetFeedbackState(FeedbackState::Listening);
    } else if (strcmp(status, "Thinking") == 0 || strcmp(status, Lang::Strings::CONNECTING) == 0) {
        SetFeedbackState(FeedbackState::Processing);
    } else if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
        SetFeedbackState(FeedbackState::Speaking);
    } else if (strcmp(status, "Cancelled") == 0) {
        SetFeedbackState(FeedbackState::Cancelled, 1500);
    } else if (strcmp(status, Lang::Strings::ERROR) == 0) {
        SetFeedbackState(FeedbackState::Error, 2000);
    } else if (strcmp(status, Lang::Strings::STANDBY) == 0) {
        SetFeedbackState(FeedbackState::Idle);
    }
}

void CustomLcdDisplay::UpdateStatusBar(bool update_all) {
    LcdDisplay::UpdateStatusBar(update_all);

    if (!IsSetupUICalled() || battery_percent_overlay_ == nullptr || battery_label_ == nullptr) {
        return;
    }

    int battery_level = 0;
    bool charging = false;
    bool discharging = false;
    if (!Board::GetInstance().GetBatteryLevel(battery_level, charging, discharging)) {
        return;
    }

    char text[8];
    snprintf(text, sizeof(text), "%d%%", battery_level);

    {
        DisplayLockGuard lock(this);
        lv_label_set_text(battery_percent_overlay_, text);
        lv_obj_align_to(battery_percent_overlay_, battery_label_, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    }

    if (!HasActiveTransientFeedback()) {
        auto state = Application::GetInstance().GetDeviceState();
        if (state == kDeviceStateListening) {
            SetFeedbackState(FeedbackState::Listening);
        } else if (state == kDeviceStateConnecting) {
            SetFeedbackState(FeedbackState::Processing);
        } else if (state == kDeviceStateSpeaking) {
            SetFeedbackState(FeedbackState::Speaking);
        } else if (!has_chat_content_ && state == kDeviceStateIdle) {
            if (feedback_state_ == FeedbackState::Cancelled || feedback_state_ == FeedbackState::Error || feedback_state_ == FeedbackState::Processing) {
                LcdDisplay::SetStatus(Lang::Strings::STANDBY);
            }
            SetFeedbackState(FeedbackState::Idle);
        }
    }
}

void CustomLcdDisplay::SetChatMessage(const char *role, const char *content) {
    LcdDisplay::SetChatMessage(role, content);

    has_chat_content_ = content != nullptr && content[0] != '\0';

    if (strcmp(role, "user") == 0 && has_chat_content_) {
        LcdDisplay::SetStatus("Thinking");
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
    } else if (strcmp(role, "assistant") == 0 && has_chat_content_) {
        SetFeedbackState(FeedbackState::Speaking);
    }

    const bool show_center_widgets = !has_chat_content_;
    UpdateOverlayVisibility(show_center_widgets);
}

void CustomLcdDisplay::ClearChatMessages() {
    LcdDisplay::ClearChatMessages();
    has_chat_content_ = false;
    if (!HasActiveTransientFeedback()) {
        SetFeedbackState(FeedbackState::Idle);
    }
    UpdateOverlayVisibility(true);
}

void CustomLcdDisplay::SetClimate(float temperature_c, float humidity_percent, bool valid) {
    const bool had_valid_read = climate_valid_;
    if (valid && isfinite(temperature_c) && isfinite(humidity_percent)) {
        cached_temperature_c_ = temperature_c;
        cached_humidity_percent_ = humidity_percent;
        climate_valid_ = true;
    }

    if (!IsSetupUICalled() || climate_label_ == nullptr) {
        return;
    }

    char text[32];
    if (valid && isfinite(temperature_c) && isfinite(humidity_percent)) {
        snprintf(text, sizeof(text), "%.1fC  %.0f%%RH", temperature_c, humidity_percent);
    } else if (had_valid_read && isfinite(cached_temperature_c_) && isfinite(cached_humidity_percent_)) {
        snprintf(text, sizeof(text), "%.1fC  %.0f%%RH", cached_temperature_c_, cached_humidity_percent_);
    } else {
        snprintf(text, sizeof(text), "Climate unavailable");
    }

    DisplayLockGuard lock(this);
    auto *lvgl_theme = static_cast<LvglTheme *>(current_theme_);
    lv_label_set_text(climate_label_, text);
    lv_obj_align_to(climate_label_, feedback_label_, LV_ALIGN_OUT_BOTTOM_MID, 0, lvgl_theme->spacing(2));
}

void CustomLcdDisplay::UpdateOverlayVisibility(bool show_center_widgets) {
    if (!IsSetupUICalled()) {
        return;
    }

    DisplayLockGuard lock(this);
    if (emoji_box_ != nullptr) {
        if (show_center_widgets) {
            lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (ai_icon_label_ != nullptr) {
        if (show_center_widgets) {
            lv_obj_remove_flag(ai_icon_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ai_icon_label_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (climate_label_ != nullptr) {
        if (show_center_widgets) {
            lv_obj_remove_flag(climate_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(climate_label_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (feedback_label_ != nullptr) {
        if (show_center_widgets) {
            lv_obj_remove_flag(feedback_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(feedback_label_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void CustomLcdDisplay::ShowCancelledFeedback() {
    SetStatus("Cancelled");
}

void CustomLcdDisplay::SetFeedbackState(FeedbackState state, int transient_ms) {
    feedback_state_ = state;
    transient_feedback_until_us_ = transient_ms > 0 ? esp_timer_get_time() + static_cast<int64_t>(transient_ms) * 1000 : 0;
    RefreshFeedbackLabel();
}

void CustomLcdDisplay::RefreshFeedbackLabel() {
    if (!IsSetupUICalled() || feedback_label_ == nullptr) {
        return;
    }

    const char* text = "";
    switch (feedback_state_) {
        case FeedbackState::Idle:
            text = "";
            break;
        case FeedbackState::Listening:
            text = "Listening";
            break;
        case FeedbackState::Processing:
            text = "Thinking";
            break;
        case FeedbackState::Speaking:
            text = "Speaking";
            break;
        case FeedbackState::Cancelled:
            text = "Cancelled";
            break;
        case FeedbackState::Error:
            text = "Error";
            break;
    }

    DisplayLockGuard lock(this);
    auto *lvgl_theme = static_cast<LvglTheme *>(current_theme_);
    lv_label_set_text(feedback_label_, text);
    lv_obj_align_to(feedback_label_, emoji_box_, LV_ALIGN_OUT_BOTTOM_MID, 0, lvgl_theme->spacing(2));
    lv_obj_align_to(climate_label_, feedback_label_, LV_ALIGN_OUT_BOTTOM_MID, 0, lvgl_theme->spacing(2));
}

bool CustomLcdDisplay::HasActiveTransientFeedback() const {
    return transient_feedback_until_us_ > 0 && esp_timer_get_time() < transient_feedback_until_us_;
}

void CustomLcdDisplay::spi_gpio_init() {
    int rst  = lcd_spi_data.rst;
    int cs   = lcd_spi_data.cs;
    int dc   = lcd_spi_data.dc;
    int busy = lcd_spi_data.busy;

    gpio_config_t gpio_conf = {};
    gpio_conf.intr_type     = GPIO_INTR_DISABLE;
    gpio_conf.mode          = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask  = (0x1ULL << rst) | (0x1ULL << dc) | (0x1ULL << cs);
    gpio_conf.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en    = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    gpio_conf.mode         = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (0x1ULL << busy);
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

    set_rst_1();
}

void CustomLcdDisplay::spi_port_init() {
    int              mosi     = lcd_spi_data.mosi;
    int              scl      = lcd_spi_data.scl;
    int              spi_host = lcd_spi_data.spi_host;
    esp_err_t        ret;
    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num      = -1;
    buscfg.mosi_io_num      = mosi;
    buscfg.sclk_io_num      = scl;
    buscfg.quadwp_io_num    = -1;
    buscfg.quadhd_io_num    = -1;
    buscfg.max_transfer_sz  = Width * Height;

    spi_device_interface_config_t devcfg = {};
    devcfg.spics_io_num                  = -1;
    devcfg.clock_speed_hz                = 40 * 1000 * 1000; // Clock out at 10 MHz
    devcfg.mode                          = 0;                // SPI mode 0
    devcfg.queue_size                    = 7;                // We want to be able to queue 7 transactions at a time

    ret = spi_bus_initialize((spi_host_device_t) spi_host, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    ret = spi_bus_add_device((spi_host_device_t) spi_host, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
}

void CustomLcdDisplay::read_busy() {
    int busy = lcd_spi_data.busy;
    while (gpio_get_level((gpio_num_t) busy) == 1) {
        vTaskDelay(pdMS_TO_TICKS(5)); // LOW: idle, HIGH: busy
    }
}

void CustomLcdDisplay::SPI_SendByte(uint8_t data) {
    esp_err_t         ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8;
    t.tx_buffer = &data;
    ret         = spi_device_polling_transmit(spi, &t); // Transmit!
    assert(ret == ESP_OK);                              // Should have had no issues.
}

void CustomLcdDisplay::EPD_SendData(uint8_t data) {
    set_dc_1();
    set_cs_0();
    SPI_SendByte(data);
    set_cs_1();
}

void CustomLcdDisplay::EPD_SendCommand(uint8_t command) {
    set_dc_0();
    set_cs_0();
    SPI_SendByte(command);
    set_cs_1();
}

void CustomLcdDisplay::writeBytes(uint8_t *buffer, int len) {
    set_dc_1();
    set_cs_0();
    esp_err_t         ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8 * len;
    t.tx_buffer = buffer;
    ret         = spi_device_polling_transmit(spi, &t); // Transmit!
    assert(ret == ESP_OK);
    set_cs_1();
}

void CustomLcdDisplay::writeBytes(const uint8_t *buffer, int len) {
    set_dc_1();
    set_cs_0();
    esp_err_t         ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length    = 8 * len;
    t.tx_buffer = buffer;
    ret         = spi_device_polling_transmit(spi, &t); // Transmit!
    assert(ret == ESP_OK);
    set_cs_1();
}

void CustomLcdDisplay::EPD_SetWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend) {
    EPD_SendCommand(0x44); // SET_RAM_X_ADDRESS_START_END_POSITION
    EPD_SendData((Xstart >> 3) & 0xFF);
    EPD_SendData((Xend >> 3) & 0xFF);

    EPD_SendCommand(0x45); // SET_RAM_Y_ADDRESS_START_END_POSITION
    EPD_SendData(Ystart & 0xFF);
    EPD_SendData((Ystart >> 8) & 0xFF);
    EPD_SendData(Yend & 0xFF);
    EPD_SendData((Yend >> 8) & 0xFF);
}

void CustomLcdDisplay::EPD_SetCursor(uint16_t Xstart, uint16_t Ystart) {
    EPD_SendCommand(0x4E); // SET_RAM_X_ADDRESS_COUNTER
    EPD_SendData(Xstart & 0xFF);

    EPD_SendCommand(0x4F); // SET_RAM_Y_ADDRESS_COUNTER
    EPD_SendData(Ystart & 0xFF);
    EPD_SendData((Ystart >> 8) & 0xFF);
}

void CustomLcdDisplay::EPD_SetLut(const uint8_t *lut) {
    EPD_SendCommand(0x32);
    writeBytes(lut, 153);
    read_busy();

    EPD_SendCommand(0x3f);
    EPD_SendData(lut[153]);

    EPD_SendCommand(0x03);
    EPD_SendData(lut[154]);

    EPD_SendCommand(0x04);
    EPD_SendData(lut[155]);
    EPD_SendData(lut[156]);
    EPD_SendData(lut[157]);

    EPD_SendCommand(0x2c);
    EPD_SendData(lut[158]);
}

void CustomLcdDisplay::EPD_TurnOnDisplay() {
    EPD_SendCommand(0x22);
    EPD_SendData(0xc7);
    EPD_SendCommand(0x20);
    read_busy();
}

void CustomLcdDisplay::EPD_TurnOnDisplayPart() {
    EPD_SendCommand(0x22);
    EPD_SendData(0xcf);
    EPD_SendCommand(0x20);
    read_busy();
}

void CustomLcdDisplay::EPD_Init() {
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));
    set_rst_0();
    vTaskDelay(pdMS_TO_TICKS(20));
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));

    read_busy();
    EPD_SendCommand(0x12); // SWRESET
    read_busy();

    EPD_SendCommand(0x01); // Driver output control
    EPD_SendData(0xC7);
    EPD_SendData(0x00);
    EPD_SendData(0x01);

    EPD_SendCommand(0x11); // data entry mode
    EPD_SendData(0x01);

    EPD_SetWindows(0, Width - 1, Height - 1, 0);

    EPD_SendCommand(0x3C); // BorderWavefrom
    EPD_SendData(0x01);

    EPD_SendCommand(0x18);
    EPD_SendData(0x80);

    EPD_SendCommand(0x22); // Load Temperature and waveform setting.
    EPD_SendData(0XB1);
    EPD_SendCommand(0x20);

    EPD_SetCursor(0, Height - 1);
    read_busy();

    EPD_SetLut(WF_Full_1IN54);
}

void CustomLcdDisplay::EPD_Clear() {
    int buffer_len = lcd_spi_data.buffer_len;
    memset(buffer, 0xff, buffer_len);
}

void CustomLcdDisplay::EPD_Display() {
    int buffer_len = lcd_spi_data.buffer_len;
    EPD_SendCommand(0x24);
    assert(buffer);
    writeBytes(buffer, buffer_len);
    EPD_TurnOnDisplay();
}

void CustomLcdDisplay::EPD_DisplayPartBaseImage() {
    int buffer_len = lcd_spi_data.buffer_len;
    EPD_SendCommand(0x24);
    assert(buffer);
    writeBytes(buffer, buffer_len);
    EPD_SendCommand(0x26);
    writeBytes(buffer, buffer_len);
    EPD_TurnOnDisplay();
}

void CustomLcdDisplay::EPD_Init_Partial() {
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));
    set_rst_0();
    vTaskDelay(pdMS_TO_TICKS(20));
    set_rst_1();
    vTaskDelay(pdMS_TO_TICKS(50));

    read_busy();

    EPD_SetLut(WF_PARTIAL_1IN54_0);

    EPD_SendCommand(0x37);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x40);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);
    EPD_SendData(0x00);

    EPD_SendCommand(0x3C); // BorderWavefrom
    EPD_SendData(0x80);

    EPD_SendCommand(0x22);
    EPD_SendData(0xc0);
    EPD_SendCommand(0x20);
    read_busy();
}

void CustomLcdDisplay::EPD_DisplayPart() {
    EPD_SendCommand(0x24);
    assert(buffer);
    writeBytes(buffer, 5000);
    EPD_TurnOnDisplayPart();
}

void CustomLcdDisplay::EPD_DrawColorPixel(uint16_t x, uint16_t y, uint8_t color) {
    if (x >= Width || y >= Height) {
        ESP_LOGE("EPD", "Out of bounds pixel: (%d,%d)", x, y);
        return;
    }

    uint16_t index = y * 25 + (x >> 3);
    uint8_t  bit   = 7 - (x & 0x07);
    if (color == DRIVER_COLOR_WHITE) {
        buffer[index] |= (0x01 << bit);
    } else {
        buffer[index] &= ~(0x01 << bit);
    }
}
