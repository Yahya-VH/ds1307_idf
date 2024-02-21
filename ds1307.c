#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/i2c.h"
#include "esp_log.h"

// WiFi configuration
#define WIFI_SSID           "my ssid"
#define WIFI_PASSWORD       "password"

#define DS1307_ADDR         0x68

#define DS1307_REG_SECONDS  0x00
#define DS1307_REG_MINUTES  0x01
#define DS1307_REG_HOURS    0x02
#define DS1307_REG_DAY      0x03
#define DS1307_REG_DATE     0x04
#define DS1307_REG_MONTH    0x05
#define DS1307_REG_YEAR     0x06

#define I2C_MASTER_SCL_IO               22
#define I2C_MASTER_SDA_IO               21
#define I2C_MASTER_NUM                  I2C_NUM_0
#define I2C_MASTER_FREQ_HZ              100000

uint8_t bcd_to_dec(uint8_t val) {
    return (val / 16 * 10) + (val % 16);
}

esp_err_t wifi_init();
esp_err_t wifi_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data);

esp_err_t wifi_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

esp_err_t wifi_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI("WiFi", "Station started");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI("WiFi", "Connected to access point");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI("WiFi", "Got IP address");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI("WiFi", "Disconnected from access point");
        esp_wifi_connect();
    }
    return ESP_OK;
}

esp_err_t initialize_i2c() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    esp_err_t err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE("I2C", "Error installing I2C driver: %d", err);
    }
    return err;
}

uint8_t ds1307_read_register(uint8_t reg) {
    uint8_t data;
    esp_err_t err;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS1307_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        ESP_LOGE("I2C", "Error sending command to read register: %d", err);
        return 0;
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS1307_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        ESP_LOGE("I2C", "Error sending command to read data: %d", err);
        return 0;
    }

    return data;
}

void print_time_task(void *pvParameters) {
    while (1) {
        uint8_t seconds = bcd_to_dec(ds1307_read_register(DS1307_REG_SECONDS));
        uint8_t minutes = bcd_to_dec(ds1307_read_register(DS1307_REG_MINUTES));
        uint8_t hours = bcd_to_dec(ds1307_read_register(DS1307_REG_HOURS));
        uint8_t day = bcd_to_dec(ds1307_read_register(DS1307_REG_DAY));
        uint8_t date = bcd_to_dec(ds1307_read_register(DS1307_REG_DATE));
        uint8_t month = bcd_to_dec(ds1307_read_register(DS1307_REG_MONTH));
        uint8_t year = bcd_to_dec(ds1307_read_register(DS1307_REG_YEAR));

        // Convert 24-hour format to 12-hour format
        uint8_t am_pm = hours >= 12 ? 'P' : 'A';
        hours = hours % 12;
        hours = hours ? hours : 12;  // Handle midnight (0 hours)

        // Print the correct date and time in 12-hour format
        printf("Todays  date: %02d-%02d-%02d\n", year, month, date);
        printf("current time: %02d:%02d:%02d %cM\n", hours, minutes, seconds, am_pm);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}



void app_main() {
    // Initialize WiFi
    ESP_ERROR_CHECK(wifi_init());

    // Initialize I2C
    ESP_ERROR_CHECK(initialize_i2c());

    // Start a task to print time
    xTaskCreate(print_time_task, "print_time_task", 2048, NULL, 5, NULL);
}

