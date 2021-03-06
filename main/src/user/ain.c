/*
 * ain.c
 *
 *  Created on: 2019-07-05 21:22
 *      Author: Jack Chen <redchenjs@live.com>
 */

#include <string.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "driver/i2s.h"

#include "core/os.h"
#include "core/app.h"
#include "chip/i2s.h"
#include "user/vfx.h"
#include "user/ain.h"

#define TAG "ain"

static uint8_t ain_mode = DEFAULT_AIN_MODE;

static void ain_task(void *pvParameters)
{
    char data[FFT_N * 4] = {0};

    ESP_LOGI(TAG, "started.");

    while (1) {
        xEventGroupWaitBits(
            user_event_group,
            AUDIO_INPUT_RUN_BIT | AUDIO_INPUT_FFT_BIT | VFX_FFT_NULL_BIT,
            pdFALSE,
            pdTRUE,
            portMAX_DELAY
        );

        size_t bytes_read = 0;
        i2s_read(CONFIG_AUDIO_INPUT_I2S_NUM, data, FFT_N * 4, &bytes_read, portMAX_DELAY);

#ifdef CONFIG_ENABLE_VFX
        // Copy data to FFT input buffer
        uint32_t idx = 0;

#ifdef CONFIG_AUDIO_INPUT_FFT_ONLY_LEFT
        int16_t data_l = 0;
        for (uint16_t k=0; k<FFT_N; k++,idx+=4) {
            data_l = data[idx+1] << 8 | data[idx];

            vfx_fft_input[k] = (float)data_l;
        }
#elif defined(CONFIG_AUDIO_INPUT_FFT_ONLY_RIGHT)
        int16_t data_r = 0;
        for (uint16_t k=0; k<FFT_N; k++,idx+=4) {
            data_r = data[idx+3] << 8 | data[idx+2];

            vfx_fft_input[k] = (float)data_r;
        }
#else
        int16_t data_l = 0, data_r = 0;
        for (uint16_t k=0; k<FFT_N; k++,idx+=4) {
            data_l = data[idx+1] << 8 | data[idx];
            data_r = data[idx+3] << 8 | data[idx+2];

            vfx_fft_input[k] = (float)((data_l + data_r) / 2);
        }
#endif

        EventBits_t uxBits = xEventGroupGetBits(user_event_group);
        if (!(uxBits & AUDIO_INPUT_RUN_BIT)) {
            memset(vfx_fft_input, 0x00, sizeof(vfx_fft_input));
        }

        xEventGroupClearBits(user_event_group, VFX_FFT_NULL_BIT);
#endif // CONFIG_ENABLE_VFX
    }
}

void ain_set_mode(uint8_t idx)
{
#ifndef CONFIG_AUDIO_INPUT_NONE
    ain_mode = idx;
    ESP_LOGI(TAG, "mode: %u", ain_mode);

    if (ain_mode) {
        xEventGroupSetBits(user_event_group, AUDIO_INPUT_RUN_BIT);
    } else {
        xEventGroupClearBits(user_event_group, AUDIO_INPUT_RUN_BIT);
    }
#endif
}

uint8_t ain_get_mode(void)
{
    return ain_mode;
}

void ain_init(void)
{
    size_t length = sizeof(uint8_t);
    app_getenv("AIN_INIT_CFG", &ain_mode, &length);

    ain_set_mode(ain_mode);

    xTaskCreatePinnedToCore(ain_task, "ainT", 1920, NULL, configMAX_PRIORITIES - 3, NULL, 0);
}
