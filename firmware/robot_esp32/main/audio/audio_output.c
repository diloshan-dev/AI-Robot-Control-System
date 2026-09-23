#include "audio_output.h"

#include <stdlib.h>
#include <string.h>

#include "driver/i2s.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "robot_config.h"

static const char *TAG = "audio_output";
static SemaphoreHandle_t audio_lock;

static uint8_t clamp_volume(uint8_t volume_hint)
{
    return volume_hint > 100 ? 100 : volume_hint;
}

void audio_output_init(void)
{
    i2s_config_t config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = ROBOT_AUDIO_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = ROBOT_AUDIO_DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
    };
    i2s_pin_config_t pins = {
        .bck_io_num = ROBOT_AUDIO_I2S_BCLK,
        .ws_io_num = ROBOT_AUDIO_I2S_LRCK,
        .data_out_num = ROBOT_AUDIO_I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE,
    };
    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &config, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM_0, &pins));
    audio_lock = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "PCM16 I2S output initialized at %d Hz", ROBOT_AUDIO_SAMPLE_RATE);
}

void audio_output_play_pcm16(const uint8_t *data, size_t length, uint8_t volume_hint)
{
    if (data == NULL || length < sizeof(int16_t) || audio_lock == NULL) {
        return;
    }
    if (xSemaphoreTake(audio_lock, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return;
    }

    const uint8_t volume = clamp_volume(volume_hint);
    const size_t sample_count = length / sizeof(int16_t);
    int16_t *scaled = calloc(sample_count, sizeof(int16_t));
    if (scaled == NULL) {
        xSemaphoreGive(audio_lock);
        ESP_LOGE(TAG, "Unable to allocate PCM playback buffer");
        return;
    }
    const int16_t *samples = (const int16_t *)data;
    for (size_t i = 0; i < sample_count; ++i) {
        scaled[i] = (int16_t)(((int32_t)samples[i] * volume) / 100);
    }

    size_t written = 0;
    while (written < sample_count * sizeof(int16_t)) {
        size_t chunk_written = 0;
        esp_err_t err = i2s_write(
            I2S_NUM_0,
            ((const uint8_t *)scaled) + written,
            sample_count * sizeof(int16_t) - written,
            &chunk_written,
            portMAX_DELAY);
        if (err != ESP_OK || chunk_written == 0) {
            ESP_LOGW(TAG, "I2S write stopped: %s", esp_err_to_name(err));
            break;
        }
        written += chunk_written;
    }
    free(scaled);
    xSemaphoreGive(audio_lock);
}

void audio_output_stop(void)
{
    if (audio_lock != NULL && xSemaphoreTake(audio_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        i2s_zero_dma_buffer(I2S_NUM_0);
        xSemaphoreGive(audio_lock);
    }
}
