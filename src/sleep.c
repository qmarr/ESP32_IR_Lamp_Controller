#include "sleep.h"

void enter_light_sleep(gpio_num_t encoder_gpio)
{
    ESP_LOGI(TAG_SLEEP, "Preparing light sleep...");

    // Wait until button is released, otherwise ESP may wake immediately
    while (gpio_get_level(encoder_gpio) == 0)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_ERROR_CHECK(gpio_wakeup_enable(encoder_gpio, GPIO_INTR_LOW_LEVEL));
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());

    ESP_LOGI(TAG_SLEEP, "Entering light sleep. Press encoder button to wake.");

    esp_err_t err = esp_light_sleep_start();

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG_SLEEP, "Light sleep failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG_SLEEP, "Woke up from light sleep");

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG_SLEEP, "Wakeup cause: %d", cause);
    gpio_wakeup_disable(encoder_gpio);
    // Prevent the same physical press from being treated as a normal short press
    vTaskDelay(pdMS_TO_TICKS(300));
}