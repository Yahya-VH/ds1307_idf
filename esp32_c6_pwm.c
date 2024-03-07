#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#define LED_PIN         GPIO_NUM_5   // Example GPIO pin for LED on ESP32-C6
#define PWM_TIMER       LEDC_TIMER_0
#define PWM_MODE        LEDC_LOW_SPEED_MODE
#define PWM_CHANNEL     LEDC_CHANNEL_0
#define PWM_RESOLUTION  LEDC_TIMER_13_BIT

void init_pwm() {
    ledc_timer_config_t timer_conf = {
        .speed_mode = PWM_MODE,
        .timer_num = PWM_TIMER,
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz = 5000, // PWM frequency in Hz
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ledc_conf = {
        .gpio_num = LED_PIN,
        .speed_mode = PWM_MODE,
        .channel = PWM_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = PWM_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&ledc_conf);
}

void fade_led(int duty) {
    ledc_set_duty(PWM_MODE, PWM_CHANNEL, duty);
    ledc_update_duty(PWM_MODE, PWM_CHANNEL);
}

void app_main() {
    // Initialize PWM
    init_pwm();

    int brightness_percentage = 0;

    while (1) {
        // Increase brightness by 10% every 10 seconds
        brightness_percentage += 20;
        if (brightness_percentage > 100)
            brightness_percentage = 0;

        // Calculate duty cycle based on percentage
        int duty = (brightness_percentage * (1 << PWM_RESOLUTION)) / 100;
        fade_led(duty);

        vTaskDelay(pdMS_TO_TICKS(5000));  // Delay for 10 seconds
    }
}
