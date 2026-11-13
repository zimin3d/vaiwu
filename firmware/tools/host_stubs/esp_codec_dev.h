/* Host-check stub. */
#pragma once
typedef struct audio_codec_data_if_t audio_codec_data_if_t;
typedef struct audio_codec_ctrl_if_t audio_codec_ctrl_if_t;
typedef struct audio_codec_gpio_if_t audio_codec_gpio_if_t;
typedef struct audio_codec_if_t audio_codec_if_t;
typedef void* esp_codec_dev_handle_t;
#ifdef __cplusplus
extern "C" {
#endif
int esp_codec_dev_read(esp_codec_dev_handle_t codec, void* data, int len);
int esp_codec_dev_set_in_gain(esp_codec_dev_handle_t codec, float db);
#ifdef __cplusplus
}
#endif
