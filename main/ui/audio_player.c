/* File-based PCM player reading from SD card. */
#include "audio_player.h"
#include "esp_log.h"
#include "esp_codec_dev.h"
#include "bsp/esp32_p4_wifi6_touch_lcd_7b.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "audio";

static const char *s_track_paths[] = {
    "/sdcard/music/song1.pcm",
    "/sdcard/music/song2.pcm",
    "/sdcard/music/song3.pcm",
    "/sdcard/music/song4.pcm",
};
static const char *s_track_names[] = {
    "Ba Hui Yi Pin Hao Gei Ni",
    "Wang Bu Diao De Ni",
    "Ni Yao De Quan Na Zou",
    "Wu Jian Du",
};
#define TRACK_COUNT (sizeof(s_track_paths)/sizeof(s_track_paths[0]))

static esp_codec_dev_handle_t s_spk = NULL;
static TaskHandle_t s_task = NULL;
static volatile bool s_playing = false;
static volatile bool s_should_stop = false;
static int s_volume = 30;
static int s_cur_idx = 0;
static volatile size_t s_cur_offset = 0;
static volatile size_t s_cur_size = 0;

static void player_task(void *arg)
{
    int idx = (int)(intptr_t)arg;
    s_playing = true;
    s_should_stop = false;
    s_cur_idx = idx;
    s_cur_offset = 0;

    FILE *f = fopen(s_track_paths[idx], "rb");
    if (!f) {
        ESP_LOGE(TAG, "open %s fail", s_track_paths[idx]);
        s_playing = false;
        s_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    s_cur_size = sz;
    ESP_LOGI(TAG, "play %s, %u bytes", s_track_names[idx], (unsigned)sz);

    uint8_t buf[4096];
    size_t sent = 0;
    while (sent < sz && !s_should_stop) {
        size_t want = sz - sent;
        if (want > sizeof(buf)) want = sizeof(buf);
        size_t rd = fread(buf, 1, want, f);
        if (rd == 0) break;
        esp_err_t ret = esp_codec_dev_write(s_spk, buf, rd);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "write: %s", esp_err_to_name(ret)); break; }
        sent += rd;
        s_cur_offset = sent;
    }
    fclose(f);
    ESP_LOGI(TAG, "done %u/%u", (unsigned)sent, (unsigned)sz);
    s_playing = false;
    s_should_stop = false;
    s_task = NULL;
    vTaskDelete(NULL);
}

void audio_player_init(void)
{
    if (s_spk) return;
    gpio_config_t pa = {
        .pin_bit_mask = (1ULL << GPIO_NUM_53),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pa);
    gpio_set_level(GPIO_NUM_53, 1);

    s_spk = bsp_audio_codec_speaker_init();
    if (!s_spk) { ESP_LOGE(TAG, "spk init fail"); return; }
    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 16000, .channel = 2, .bits_per_sample = 16,
    };
    if (esp_codec_dev_open(s_spk, &fs) != ESP_OK) { ESP_LOGE(TAG, "open fail"); s_spk=NULL; return; }
    esp_codec_dev_set_out_vol(s_spk, s_volume);
    ESP_LOGI(TAG, "ready, vol=%d, tracks=%d", s_volume, (int)TRACK_COUNT);
}

int audio_player_track_count(void) { return (int)TRACK_COUNT; }
int audio_player_track_index(void) { return s_cur_idx; }
const char *audio_player_track_name(int i) {
    if (i < 0 || i >= (int)TRACK_COUNT) return "?";
    return s_track_names[i];
}

int audio_player_track_duration_sec(void) {
    return (int)(s_cur_size / 64000);
}

int audio_player_position_sec(void) {
    return (int)(s_cur_offset / 64000);
}

void audio_player_play_track(int idx)
{
    if (!s_spk) return;
    if (idx < 0 || idx >= (int)TRACK_COUNT) idx = 0;
    if (s_task) { s_should_stop = true; while (s_task) vTaskDelay(pdMS_TO_TICKS(20)); }
    xTaskCreate(player_task, "audio_player", 16384, (void *)(intptr_t)idx, 5, &s_task);
}

void audio_player_play(void) { audio_player_play_track(s_cur_idx); }
void audio_player_next(void) { audio_player_play_track((s_cur_idx + 1) % (int)TRACK_COUNT); }
void audio_player_prev(void) { audio_player_play_track((s_cur_idx - 1 + (int)TRACK_COUNT) % (int)TRACK_COUNT); }
void audio_player_pause(void) { s_should_stop = true; }
void audio_player_stop(void)  { s_should_stop = true; }
bool audio_player_is_playing(void) { return s_playing; }

void audio_player_set_volume(int vol)
{
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    s_volume = vol;
    if (s_spk) esp_codec_dev_set_out_vol(s_spk, vol);
}
