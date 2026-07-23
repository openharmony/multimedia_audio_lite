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

#include "audio_source_virtual.h"
#include "media_log.h"
#include "media_info.h"
#include "media_errors.h"
#include "securec.h"
#include "audio_manager_interface_impl.h"
#include "audio_adapter_interface_impl.h"
#include "audio_capture_interface_impl.h"

namespace OHOS {
namespace Audio {
using namespace OHOS::Media;
using OHOS::HDI::DistributedAudio::Audio::V1_0::AudioAdapterDescriptor;
void AudioSourceVirtual::AudioAdapterDescriptorMatch(const AudioAdapterDescriptor &desc)
{
    for (uint32_t port = 0; port < desc.ports.size(); port++) {
            if (desc.ports[port].dir == PORT_OUT_IN \
                 && !(audioManager_->LoadAdapter(desc, audioAdapter_))) {
                if (audioAdapter_ == nullptr) {
                    MEDIA_ERR_LOG("LoadAdapter audioAdapter_ is nullptr");
                    continue;
                }
                (void)audioAdapter_->InitAllPorts();
                if (deviceId_ == desc.adapterName || deviceId_ == "") {
                    adapterName_ = desc.adapterName;
                    MEDIA_INFO_LOG("LoadAdapter adapterName_  = %s!", adapterName_.c_str());
                    return;
                }
                capturePort_.push_back(desc.ports[port]);
            }
        }
}
AudioSourceVirtual::AudioSourceVirtual(std::string deviceId)
    : initialized_(false),
      started_(false),
      deviceId_(deviceId),
      adapterName_(""),
      audioAdapter_(nullptr),
      audioCapture_(nullptr)
{
    if (audioManager_ == nullptr) {
        audioManager_ = AudioManagerInterfaceImpl::GetAudioManager();
        MEDIA_DEBUG_LOG("g_audioManager");
    }
    std::vector<OHOS::HDI::DistributedAudio::Audio::V1_0::AudioAdapterDescriptor> vecDescs;
    audioManager_->GetAllAdapters(vecDescs);
    MEDIA_DEBUG_LOG("GetAllAdapters size:%d", vecDescs.size());

    for (uint32_t index = 0; index < vecDescs.size(); index++) {
        OHOS::HDI::DistributedAudio::Audio::V1_0::AudioAdapterDescriptor desc = vecDescs[index];
        AudioAdapterDescriptorMatch(desc);
        if (!adapterName_.empty()) {
            break;
        }
    }
    if (audioAdapter_ == nullptr) {
        MEDIA_ERR_LOG("LoadAdapter audioAdapter_ failed!");
    }
    MEDIA_DEBUG_LOG("LoadAdapter audioAdapter_");
}

AudioSourceVirtual::~AudioSourceVirtual()
{
    MEDIA_DEBUG_LOG("in");
    if (initialized_) {
        Release();
    }
    
    if (audioAdapter_ != nullptr) {
        MEDIA_INFO_LOG("UnloadModule audioAdapter_");
        if (audioManager_ == nullptr) {
            MEDIA_ERR_LOG("~AudioSource audioManager_ is nullptr");
            audioAdapter_ = nullptr;
            return;
        }
        audioManager_->UnloadAdapter(adapterName_);
        audioAdapter_ = nullptr;
    }
}

int32_t AudioSourceVirtual::InitCheck()
{
    if (!initialized_) {
        MEDIA_ERR_LOG("not initialized");
        return ERR_ILLEGAL_STATE;
    }
    return SUCCESS;
}

uint64_t AudioSourceVirtual::GetFrameCount()
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
    ret = audioCapture_->GetFrameCount(frameCount);
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("attr GetFrameCount failed:0x%x ", ret);
        return ret;
    }
    return frameCount;
}

int32_t AudioSourceVirtual::EnumDeviceBySourceType(AudioSourceType inputSource, std::vector<AudioDeviceDesc> &devices)
{
    if (inputSource != AUDIO_MIC && inputSource != AUDIO_SOURCE_DEFAULT) {
        MEDIA_ERR_LOG("AudioSource only support AUDIO_MIC");
        return ERR_INVALID_PARAM;
    }
    if (audioAdapter_ == nullptr) {
        MEDIA_ERR_LOG("audioAdapter_ is NULL");
        return ERR_ILLEGAL_STATE;
    }

    OHOS::HDI::DistributedAudio::Audio::V1_0::AudioPortCapability capability;
    OHOS::HDI::DistributedAudio::Audio::V1_0::AudioPort port = {};
    if (capturePort_.size() > 0) {
        port = capturePort_[0];
    }
    audioAdapter_->GetPortCapability(port, capability);
    AudioDeviceDesc deviceDesc;
    deviceDesc.deviceId = capability.deviceId;
    deviceDesc.inputSourceType = AUDIO_MIC;
    devices.push_back(deviceDesc);
    MEDIA_INFO_LOG("EnumDeviceBySourceType success");
    return SUCCESS;
}

static bool ConvertCodecFormatToAudioFormat(AudioCodecFormat codecFormat,
    OHOS::HDI::DistributedAudio::Audio::V1_0::AudioFormat *audioFormat)
{
    if (audioFormat == nullptr) {
        MEDIA_ERR_LOG("audioFormat is NULL");
        return false;
    }

    switch (codecFormat) {
        case AUDIO_DEFAULT:
        case PCM:
            *audioFormat = OHOS::HDI::DistributedAudio::Audio::V1_0::AUDIO_FORMAT_TYPE_PCM_16_BIT;
            break;
        case AAC_LC:
            *audioFormat = OHOS::HDI::DistributedAudio::Audio::V1_0::AUDIO_FORMAT_TYPE_AAC_LC;
            break;
        case AAC_LD:
            *audioFormat = OHOS::HDI::DistributedAudio::Audio::V1_0::AUDIO_FORMAT_TYPE_AAC_LD;
            break;
        case AAC_ELD:
            *audioFormat = OHOS::HDI::DistributedAudio::Audio::V1_0::AUDIO_FORMAT_TYPE_AAC_ELD;
            break;
        case AAC_HE_V1:
            *audioFormat = OHOS::HDI::DistributedAudio::Audio::V1_0::AUDIO_FORMAT_TYPE_AAC_HE_V1;
            break;
        case AAC_HE_V2:
            *audioFormat = OHOS::HDI::DistributedAudio::Audio::V1_0::AUDIO_FORMAT_TYPE_AAC_HE_V2;
            break;
        case G711A:
            *audioFormat = OHOS::HDI::DistributedAudio::Audio::V1_0::AUDIO_FORMAT_TYPE_G711A;
            break;
        case G711U:
            *audioFormat = OHOS::HDI::DistributedAudio::Audio::V1_0::AUDIO_FORMAT_TYPE_G711U;
            break;
        case G726:
            *audioFormat = OHOS::HDI::DistributedAudio::Audio::V1_0::AUDIO_FORMAT_TYPE_G726;
            break;
        default: {
            MEDIA_ERR_LOG("not support this codecFormat:%d", codecFormat);
            return false;
        }
    }
    return true;
}

int32_t AudioSourceVirtual::Initialize(const AudioSourceConfig &config)
{
    if (audioAdapter_ == nullptr) {
        MEDIA_ERR_LOG("audioAdapter_ is NULL");
        return ERR_ILLEGAL_STATE;
    }
    MEDIA_INFO_LOG("deviceId:0x%x config.sampleRate:%d", config.deviceId, config.sampleRate);
    OHOS::HDI::DistributedAudio::Audio::V1_0::AudioDeviceDescriptor desc;
    desc.pins = PIN_IN_MIC;
    desc.desc = "";
    OHOS::HDI::DistributedAudio::Audio::V1_0::AudioSampleAttributes attrs;
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
    attrs.sampleRate = config.sampleRate;
    attrs.channelCount = config.channelCount;
    attrs.interleaved = config.interleaved;
    int32_t ret = audioAdapter_->CreateCapture(desc, attrs, audioCapture_);
    if (ret != SUCCESS || audioCapture_ == nullptr) {
        MEDIA_ERR_LOG("CreateCapture failed:0x%x", ret);
        return ret;
    }
    initialized_ = true;
    return SUCCESS;
}

int32_t AudioSourceVirtual::SetInputDevice(uint32_t deviceId)
{
    (void)deviceId;
    return SUCCESS;
}

int32_t AudioSourceVirtual::GetCurrentDeviceId(uint32_t &deviceId)
{
    if (audioCapture_ == nullptr) {
        MEDIA_ERR_LOG("audioCapture_ is NULL");
        return ERR_ILLEGAL_STATE;
    }
    int32_t ret = audioCapture_->GetCurrentChannelId(deviceId);
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("GetCurrentChannelId failed:0x%x", ret);
        return ret;
    }
    MEDIA_INFO_LOG("deviceId:0x%x", deviceId);
    return SUCCESS;
}

int32_t AudioSourceVirtual::Start()
{
    int32_t ret;
    if ((ret = InitCheck()) != SUCCESS) {
        return ret;
    }

    if (audioCapture_ == nullptr) {
        MEDIA_ERR_LOG("audioCapture_ is NULL");
        return ERR_ILLEGAL_STATE;
    }
    ret = audioCapture_->Start();
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("audioCapture_ Start failed:0x%x", ret);
        return ret;
    }
    started_ = true;
    return SUCCESS;
}

int32_t AudioSourceVirtual::ReadFrame(AudioFrame &frame, bool isBlockingRead)
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
    std::vector<int8_t> vecframe;
    int32_t ret = audioCapture_->CaptureFrame(vecframe, frame.bufferLen);
    if (ret != SUCCESS) {
        if (ret == ERR_AUDIO_READ_DATA_TIME_OUT) {
            return ret;
        }
        MEDIA_ERR_LOG("audioCapture_::CaptureFrame failed:0x%x", ret);
        return ERR_INVALID_READ;
    }
    OHOS::HDI::DistributedAudio::Audio::V1_0::AudioTimeStamp timeStamp = {};
    ret = audioCapture_->GetCapturePosition(frame.frames, timeStamp);
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("audioCapture_::GetCapturePosition failed:0x%x", ret);
        return ERR_INVALID_READ;
    }
    frame.time.tvSec = timeStamp.tvSec;
    frame.time.tvNSec = timeStamp.tvNSec;
    readlen = vecframe.size();
    if (readlen > 0) {
        (void)memcpy_s(frame.buffer, frame.bufferLen, vecframe.data(), readlen);
    }
    return readlen;
}

int32_t AudioSourceVirtual::Stop()
{
    MEDIA_INFO_LOG("AudioSourceVirtual::Stop");
    if (!started_) {
        MEDIA_ERR_LOG("AudioSource not Start");
        return ERR_ILLEGAL_STATE;
    }

    if (audioCapture_ == nullptr) {
        MEDIA_ERR_LOG("audioCapture_ is NULL");
        return ERR_ILLEGAL_STATE;
    }
    int32_t ret = audioCapture_->Stop();
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("Stop failed:0x%x", ret);
        return ret;
    }
    started_ = false;
    return SUCCESS;
}

int32_t AudioSourceVirtual::Release()
{
    int32_t ret;
    if ((ret = InitCheck()) != SUCCESS) {
        return ret;
    }
    if (audioCapture_) {
        OHOS::HDI::DistributedAudio::Audio::V1_0::AudioDeviceDescriptor desc;
        desc.pins = PIN_IN_MIC;
        desc.desc = "";
        audioAdapter_->DestroyCapture(desc);
        audioCapture_ = nullptr;
    }
    initialized_ = false;
    MEDIA_INFO_LOG("AudioSource Released");
    return SUCCESS;
}

void AudioSourceVirtual::SetDeviceChangeCallback(const std::shared_ptr<IAudioManagerDeviceChangeCallback> &callback)
{
    audioManager_->SetDeviceChangeCallback(callback);
}

}  // namespace Audio
}  // namespace OHOS
