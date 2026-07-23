/*
 * Copyright (c) 2025 HiSilicon (Shanghai) Technologies Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio_source_local.h"
#include <algorithm>
#include "media_log.h"
#include "securec.h"

namespace OHOS {
namespace Audio {
using namespace OHOS::Media;
static AudioManager *g_audioManager = nullptr;
AudioSourceLocal::AudioSourceLocal()
    : initialized_(false),
      started_(false),
      audioAdapter_(nullptr),
      audioCapture_(nullptr)
{
    if (g_audioManager == nullptr) {
        g_audioManager = GetAudioManagerFuncs();
        MEDIA_DEBUG_LOG("g_audioManager");
    }
    if (g_audioManager == nullptr) {
        MEDIA_ERR_LOG("AudioSourceLocal g_audioManager is nullptr");
        return;
    }
    int size = 0;
    struct AudioAdapterDescriptor *descs = nullptr;
    g_audioManager->GetAllAdapters(g_audioManager, &descs, &size);
    MEDIA_DEBUG_LOG("GetAllAdapters size:%d", size);

    for (int index = 0; index < size; index++) {
        struct AudioAdapterDescriptor *desc = &descs[index];
        if (strcmp(desc->adapterName, "Primary") == 0) {  // USB
            IsPrimaryAdapter(desc);
        }
    }
    MEDIA_DEBUG_LOG("LoadAdapter audioAdapter_");
}

void AudioSourceLocal::IsPrimaryAdapter(struct AudioAdapterDescriptor *desc)
{
    if (desc == nullptr) {
        return;
    }
    for (int port = 0; port < static_cast<int>(desc->portNum); port++) {
        if (desc->ports[port].dir == PORT_IN \
             && !(g_audioManager->LoadAdapter(g_audioManager, desc, &audioAdapter_))) {
            (void)audioAdapter_->InitAllPorts(audioAdapter_);
            if (memcpy_s(&capturePort_, sizeof(struct AudioPort),
                &desc->ports[port], sizeof(struct AudioPort))) {
                MEDIA_WARNING_LOG("memcpy_s capturePort_ failed");
            }
            return;
        }
    }
}

AudioSourceLocal::~AudioSourceLocal()
{
    MEDIA_DEBUG_LOG("in");
    if (initialized_) {
        Release();
    }

    if (audioAdapter_ != nullptr) {
        MEDIA_INFO_LOG("UnloadModule audioAdapter_");
        if (g_audioManager == nullptr) {
            MEDIA_ERR_LOG("~AudioSource g_audioManager is nullptr");
            audioAdapter_ = nullptr;
            return;
        }
        g_audioManager->UnloadAdapter(g_audioManager, audioAdapter_);
        audioAdapter_ = nullptr;
    }
}

int32_t AudioSourceLocal::InitCheck()
{
    if (!initialized_) {
        MEDIA_ERR_LOG("not initialized");
        return ERR_ILLEGAL_STATE;
    }
    return SUCCESS;
}

uint64_t AudioSourceLocal::GetFrameCount()
{
    int32_t ret;
    if ((ret = InitCheck()) != SUCCESS) {
        return ret;
    }
    if (audioCapture_ == nullptr) {
        MEDIA_ERR_LOG("audioCapture_ is NULL");
        return ERR_ILLEGAL_STATE;
    }
    uint64_t frameCount = 0;
    ret = audioCapture_->attr.GetFrameCount(reinterpret_cast<AudioHandle>(audioCapture_), &frameCount);
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("attr GetFrameCount failed:0x%x ", ret);
        return ret;
    }
    return frameCount;
}

int32_t AudioSourceLocal::EnumDeviceBySourceType(AudioSourceType inputSource, std::vector<AudioDeviceDesc> &devices)
{
    if (inputSource != AUDIO_MIC && inputSource != AUDIO_SOURCE_DEFAULT) {
        MEDIA_ERR_LOG("AudioSource only support AUDIO_MIC");
        return ERR_INVALID_PARAM;
    }
    if (audioAdapter_ == nullptr) {
        MEDIA_ERR_LOG("audioAdapter_ is NULL");
        return ERR_ILLEGAL_STATE;
    }

    struct AudioPortCapability capability;
    audioAdapter_->GetPortCapability(audioAdapter_, &capturePort_, &capability);
    AudioDeviceDesc deviceDesc;
    deviceDesc.deviceId = capability.deviceId;
    deviceDesc.inputSourceType = AUDIO_MIC;
    devices.push_back(deviceDesc);
    return SUCCESS;
}

static bool ConvertCodecFormatToAudioFormat(AudioCodecFormat codecFormat, AudioFormat *audioFormat)
{
    if (audioFormat == nullptr) {
        MEDIA_ERR_LOG("audioFormat is NULL");
        return false;
    }

    switch (codecFormat) {
        case AUDIO_DEFAULT:
        case PCM:
            *audioFormat = AUDIO_FORMAT_TYPE_PCM_16_BIT;
            break;
        case AAC_LC:
            *audioFormat = AUDIO_FORMAT_TYPE_AAC_LC;
            break;
        case AAC_LD:
            *audioFormat = AUDIO_FORMAT_TYPE_AAC_LD;
            break;
        case AAC_ELD:
            *audioFormat = AUDIO_FORMAT_TYPE_AAC_ELD;
            break;
        case AAC_HE_V1:
            *audioFormat = AUDIO_FORMAT_TYPE_AAC_HE_V1;
            break;
        case AAC_HE_V2:
            *audioFormat = AUDIO_FORMAT_TYPE_AAC_HE_V2;
            break;
        case G711A:
            *audioFormat = AUDIO_FORMAT_TYPE_G711A;
            break;
        case G711U:
            *audioFormat = AUDIO_FORMAT_TYPE_G711U;
            break;
        case G726:
            *audioFormat = AUDIO_FORMAT_TYPE_G726;
            break;
        default: {
            MEDIA_ERR_LOG("not support this codecFormat:%d", codecFormat);
            return false;
        }
    }
    return true;
}

int32_t AudioSourceLocal::Initialize(const AudioSourceConfig &config)
{
    if (audioAdapter_ == nullptr) {
        MEDIA_ERR_LOG("audioAdapter_ is NULL");
        return ERR_ILLEGAL_STATE;
    }
    MEDIA_INFO_LOG("deviceId:0x%x config.sampleRate:%d", config.deviceId, config.sampleRate);
    AudioDeviceDescriptor desc;
    desc.pins = PIN_IN_MIC; // in mic:PIN_IN_MIC usb mic:PIN_IN_USB_EXT
    desc.desc = NULL;
    AudioSampleAttributes attrs;
    deviceId_ = config.deviceId;
    if (config.streamUsage == TYPE_MEDIA || config.streamUsage == TYPE_DEFAULT) {
        attrs.type = AUDIO_IN_MEDIA;
    } else if (config.streamUsage == TYPE_VOICE_COMMUNICATION) {
        attrs.type = AUDIO_IN_COMMUNICATION;
    }
    if (config.bitWidth != BIT_WIDTH_16) {
        MEDIA_ERR_LOG("not support bitWidth:%d, only support 16 bit width", config.bitWidth);
        return ERR_INVALID_PARAM;
    }
    if (!ConvertCodecFormatToAudioFormat(config.audioFormat, &(attrs.format))) {
        MEDIA_ERR_LOG("not support audioFormat:%d", config.audioFormat);
        return ERR_INVALID_PARAM;
    }
    // usb mic sampleRate 48000
    attrs.sampleRate = config.sampleRate;
    attrs.channelCount = config.channelCount;
    attrs.interleaved = config.interleaved;
    int32_t ret = audioAdapter_->CreateCapture(audioAdapter_, &desc, &attrs, &audioCapture_);
    if (ret != SUCCESS || audioCapture_ == nullptr) {
        MEDIA_ERR_LOG("CreateCapture failed:0x%x", ret);
        return ret;
    }
    initialized_ = true;
    return SUCCESS;
}

int32_t AudioSourceLocal::SetInputDevice(uint32_t deviceId)
{
    (void)deviceId;
    return SUCCESS;
}

int32_t AudioSourceLocal::GetCurrentDeviceId(uint32_t &deviceId)
{
    if (audioCapture_ == nullptr) {
        MEDIA_ERR_LOG("audioCapture_ is NULL");
        return ERR_ILLEGAL_STATE;
    }
    int32_t ret = audioCapture_->attr.GetCurrentChannelId(reinterpret_cast<AudioHandle>(audioCapture_), &deviceId);
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("GetCurrentChannelId failed:0x%x", ret);
        return ret;
    }
    MEDIA_INFO_LOG("deviceId:0x%x", deviceId);
    return SUCCESS;
}

int32_t AudioSourceLocal::Start()
{
    int32_t ret;
    if ((ret = InitCheck()) != SUCCESS) {
        return ret;
    }

    if (audioCapture_ == nullptr) {
        MEDIA_ERR_LOG("audioCapture_ is NULL");
        return ERR_ILLEGAL_STATE;
    }
    ret = audioCapture_->control.Start(reinterpret_cast<AudioHandle>(audioCapture_));
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("audioCapture_ Start failed:0x%x", ret);
        return ret;
    }
    started_ = true;
    return SUCCESS;
}

int32_t AudioSourceLocal::ReadFrame(AudioFrame &frame, bool isBlockingRead)
{
    if (!started_) {
        MEDIA_ERR_LOG("AudioSource not Start");
        return ERR_ILLEGAL_STATE;
    }
    if (audioCapture_ == nullptr) {
        MEDIA_ERR_LOG("audioCapture_ is NULL");
        return ERR_ILLEGAL_STATE;
    }
    uint64_t readlen = ERR_INVALID_READ;
    int32_t ret = audioCapture_->CaptureFrame(audioCapture_, frame.buffer, frame.bufferLen, &readlen);
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("audioCapture_::CaptureFrame failed:0x%x", ret);
        return ERR_INVALID_READ;
    }
    ret = audioCapture_->GetCapturePosition(audioCapture_, &frame.frames, &frame.time);
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("audioCapture_::GetCapturePosition failed:0x%x", ret);
        return ERR_INVALID_READ;
    }
    return readlen;
}

int32_t AudioSourceLocal::Stop()
{
    MEDIA_INFO_LOG("AudioSourceLocal::Stop");
    if (!started_) {
        MEDIA_ERR_LOG("AudioSource not Start");
        return ERR_ILLEGAL_STATE;
    }

    if (audioCapture_ == nullptr) {
        MEDIA_ERR_LOG("audioCapture_ is NULL");
        return ERR_ILLEGAL_STATE;
    }
    int32_t ret = audioCapture_->control.Stop(reinterpret_cast<AudioHandle>(audioCapture_));
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("Stop failed:0x%x", ret);
        return ret;
    }
    started_ = false;
    return SUCCESS;
}

int32_t AudioSourceLocal::Release()
{
    int32_t ret;
    if ((ret = InitCheck()) != SUCCESS) {
        return ret;
    }
    if (audioCapture_) {
        if (audioAdapter_ == nullptr) {
            MEDIA_ERR_LOG("audioAdapter_ is NULL");
            return ERR_ILLEGAL_STATE;
        }
        audioAdapter_->DestroyCapture(audioAdapter_, audioCapture_);
        audioCapture_ = nullptr;
    }
    initialized_ = false;
    MEDIA_INFO_LOG("AudioSource Released");
    return SUCCESS;
}
}  // namespace Audio
}  // namespace OHOS
