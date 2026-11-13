#include "vaiwu/hal/esp/Es8311Audio.hpp"

#ifdef ESP_PLATFORM
#include <driver/gpio.h>
#include <es8311_codec.h>
#endif

namespace vaiwu::hal::esp {

namespace {
#ifdef ESP_PLATFORM
// paActiveLow flips which logic level enables the amplifier. See the
// constructor comment in Es8311Audio.hpp: this board's PA enable pin is
// active-LOW, matching the confirmed-working xiaozhi-esp32 reference
// firmware's pa_inverted=true for this exact board.
int paEnabledLevel(bool paActiveLow)
{
    return paActiveLow ? 0 : 1;
}

int paDisabledLevel(bool paActiveLow)
{
    return paActiveLow ? 1 : 0;
}
#endif
} // namespace

Es8311Audio::Es8311Audio(ILogger& logger,
    void* i2cBusHandle,
    int i2sMclk, int i2sBclk, int i2sWs, int i2sDout, int i2sDin,
    int paPin,
    std::uint32_t sampleRateHz,
    std::uint8_t codecI2cAddr,
    bool paActiveLow)
    : logger_(logger)
    , i2cBusHandle_(i2cBusHandle)
    , i2sMclk_(i2sMclk)
    , i2sBclk_(i2sBclk)
    , i2sWs_(i2sWs)
    , i2sDout_(i2sDout)
    , i2sDin_(i2sDin)
    , paPin_(paPin)
    , sampleRateHz_(sampleRateHz)
    , codecI2cAddr_(codecI2cAddr)
    , paActiveLow_(paActiveLow)
{
}

Es8311Audio::~Es8311Audio()
{
#ifdef ESP_PLATFORM
    if (paPin_ >= 0 && initialized_) {
        gpio_set_level(static_cast<gpio_num_t>(paPin_), paDisabledLevel(paActiveLow_));
    }
    if (dev_ != nullptr) {
        esp_codec_dev_close(dev_);
        esp_codec_dev_delete(dev_);
    }
    if (codecIf_ != nullptr) {
        audio_codec_delete_codec_if(codecIf_);
    }
    if (ctrlIf_ != nullptr) {
        audio_codec_delete_ctrl_if(ctrlIf_);
    }
    if (gpioIf_ != nullptr) {
        audio_codec_delete_gpio_if(gpioIf_);
    }
    if (dataIf_ != nullptr) {
        audio_codec_delete_data_if(dataIf_);
    }
    if (txHandle_ != nullptr) {
        i2s_channel_disable(txHandle_);
        i2s_del_channel(txHandle_);
    }
    if (rxHandle_ != nullptr) {
        i2s_channel_disable(rxHandle_);
        i2s_del_channel(rxHandle_);
    }
    // i2cBusHandle_ is owned by SharedI2cBus, not by this class - never deleted here.
#endif
}

bool Es8311Audio::init()
{
#ifndef ESP_PLATFORM
    logger_.error("Es8311Audio requires ESP-IDF; not available on host builds");
    return false;
#else
    if (initialized_) {
        return true;
    }

    if (i2cBusHandle_ == nullptr) {
        logger_.error("Es8311Audio initialized with no shared I2C bus");
        return false;
    }

    if (paPin_ >= 0) {
        gpio_config_t paConfig = {};
        paConfig.pin_bit_mask = 1ULL << paPin_;
        paConfig.mode = GPIO_MODE_OUTPUT;
        gpio_config(&paConfig);
        // Keep the amplifier disabled while I2S/codec bring-up is still in
        // progress, to avoid pops.
        gpio_set_level(static_cast<gpio_num_t>(paPin_), paDisabledLevel(paActiveLow_));
    }

    i2s_chan_config_t chanConfig = {};
    chanConfig.id = I2S_NUM_0;
    chanConfig.role = I2S_ROLE_MASTER;
    chanConfig.dma_desc_num = 6;
    chanConfig.dma_frame_num = 240;
    chanConfig.auto_clear_after_cb = true;

    if (i2s_new_channel(&chanConfig, &txHandle_, &rxHandle_) != ESP_OK) {
        logger_.error("Failed to create I2S channel for audio codec");
        return false;
    }

    i2s_std_config_t stdConfig = {};
    stdConfig.clk_cfg.sample_rate_hz = sampleRateHz_;
    stdConfig.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    stdConfig.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    stdConfig.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
    stdConfig.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
    stdConfig.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
    stdConfig.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
    stdConfig.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_16BIT;
    stdConfig.slot_cfg.ws_pol = false;
    stdConfig.slot_cfg.bit_shift = true;
    stdConfig.gpio_cfg.mclk = static_cast<gpio_num_t>(i2sMclk_);
    stdConfig.gpio_cfg.bclk = static_cast<gpio_num_t>(i2sBclk_);
    stdConfig.gpio_cfg.ws = static_cast<gpio_num_t>(i2sWs_);
    stdConfig.gpio_cfg.dout = static_cast<gpio_num_t>(i2sDout_);
    stdConfig.gpio_cfg.din = static_cast<gpio_num_t>(i2sDin_);

    if (i2s_channel_init_std_mode(txHandle_, &stdConfig) != ESP_OK
        || i2s_channel_init_std_mode(rxHandle_, &stdConfig) != ESP_OK) {
        logger_.error("Failed to configure I2S standard mode for audio codec");
        return false;
    }
    if (i2s_channel_enable(txHandle_) != ESP_OK || i2s_channel_enable(rxHandle_) != ESP_OK) {
        logger_.error("Failed to enable I2S channels for audio codec");
        return false;
    }

    audio_codec_i2s_cfg_t i2sDataCfg = {};
    i2sDataCfg.port = I2S_NUM_0;
    i2sDataCfg.rx_handle = rxHandle_;
    i2sDataCfg.tx_handle = txHandle_;
    dataIf_ = audio_codec_new_i2s_data(&i2sDataCfg);

    audio_codec_i2c_cfg_t i2cCtrlCfg = {};
    i2cCtrlCfg.port = I2C_NUM_0;
    i2cCtrlCfg.addr = codecI2cAddr_;
    i2cCtrlCfg.bus_handle = static_cast<i2c_master_bus_handle_t>(i2cBusHandle_);
    ctrlIf_ = audio_codec_new_i2c_ctrl(&i2cCtrlCfg);

    gpioIf_ = audio_codec_new_gpio();

    if (dataIf_ == nullptr || ctrlIf_ == nullptr || gpioIf_ == nullptr) {
        logger_.error("Failed to create audio codec control interfaces");
        return false;
    }

    es8311_codec_cfg_t codecCfg = {};
    codecCfg.ctrl_if = ctrlIf_;
    codecCfg.gpio_if = gpioIf_;
    codecCfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    codecCfg.pa_pin = static_cast<std::int16_t>(paPin_);
    // Must match the board's actual PA enable polarity: this board's PA pin
    // is active-LOW (see Es8311Audio.hpp's constructor comment), so
    // pa_reverted has to be true here, not false.
    codecCfg.pa_reverted = paActiveLow_;
    codecCfg.use_mclk = true;
    codecCfg.hw_gain.pa_voltage = 5.0F;
    codecCfg.hw_gain.codec_dac_voltage = 3.3F;
    codecIf_ = es8311_codec_new(&codecCfg);

    if (codecIf_ == nullptr) {
        logger_.error("Failed to initialize ES8311 codec");
        return false;
    }

    esp_codec_dev_cfg_t devCfg = {};
    // IN_OUT: same codec device is used for both speaker playback and
    // microphone capture (push-to-talk voice commands).
    devCfg.dev_type = ESP_CODEC_DEV_TYPE_IN_OUT;
    devCfg.codec_if = codecIf_;
    devCfg.data_if = dataIf_;
    dev_ = esp_codec_dev_new(&devCfg);
    if (dev_ == nullptr) {
        logger_.error("Failed to create audio codec device");
        return false;
    }

    esp_codec_dev_sample_info_t sampleInfo = {};
    sampleInfo.bits_per_sample = 16;
    sampleInfo.channel = 1;
    sampleInfo.sample_rate = sampleRateHz_;
    if (esp_codec_dev_open(dev_, &sampleInfo) != ESP_OK) {
        logger_.error("Failed to open audio codec device");
        return false;
    }
    if (esp_codec_dev_set_out_vol(dev_, 70) != ESP_OK) {
        logger_.warn("Failed to set initial codec output volume");
    }
    // Analog mic gain for the ES8311 ADC (dB). 30 dB is a sane default for
    // an on-board electret; too high clips, too low loses quiet speech.
    if (esp_codec_dev_set_in_gain(dev_, 30.0F) != ESP_OK) {
        logger_.warn("Failed to set initial codec input gain");
    }

    if (paPin_ >= 0) {
        // Codec is fully configured now - safe to enable the amplifier.
        gpio_set_level(static_cast<gpio_num_t>(paPin_), paEnabledLevel(paActiveLow_));
    }

    initialized_ = true;
    return true;
#endif
}

bool Es8311Audio::startCapture()
{
    // The codec is opened IN_OUT at init(), so capture needs no extra setup;
    // callers stream audio via readMic(). Kept for IAudio compatibility.
    return initialized_;
}

AudioBuffer Es8311Audio::stopCapture()
{
    return {};
}

bool Es8311Audio::readMic(std::uint8_t* dst, std::size_t bytes)
{
#ifndef ESP_PLATFORM
    static_cast<void>(dst);
    static_cast<void>(bytes);
    return false;
#else
    if (!initialized_ || dev_ == nullptr || dst == nullptr || bytes == 0) {
        return false;
    }
    return esp_codec_dev_read(dev_, dst, static_cast<int>(bytes)) == ESP_OK;
#endif
}

bool Es8311Audio::play(const AudioBuffer& buffer)
{
#ifndef ESP_PLATFORM
    static_cast<void>(buffer);
    return false;
#else
    if (!initialized_ || dev_ == nullptr || buffer.empty()) {
        return false;
    }

    return esp_codec_dev_write(dev_, const_cast<std::uint8_t*>(buffer.data()), static_cast<int>(buffer.size())) == ESP_OK;
#endif
}

void Es8311Audio::setVolume(std::uint8_t percent)
{
#ifdef ESP_PLATFORM
    if (dev_ == nullptr) {
        return;
    }
    esp_codec_dev_set_out_vol(dev_, percent > 100 ? 100 : percent);
#else
    static_cast<void>(percent);
#endif
}

} // namespace vaiwu::hal::esp
