/*
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd.
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

#include "audio_capturer_impl.h"

#include <sys/select.h>

#ifdef MEDIA_INTERFACE_V1_0
#include "audio_source_virtual.h"
#include "audio_manager_interface_impl.h"
#endif
#include "audio_source_local.h"
#include "audio_encoder.h"
#include "media_log.h"

namespace OHOS {
namespace Audio {
using namespace OHOS::Media;

const uint64_t TIME_CONVERSION_US_S = 1000000; /* us to s */
const uint64_t TIME_CONVERSION_NS_US = 1000; /* ns  to us  */

#define CHK_NULL_RETURN(ptr, ret) \
    do { \
        if ((ptr) == nullptr) { \
            MEDIA_ERR_LOG("ptr null"); \
            return (ret); \
        } \
    } while (0)

AudioCapturerImpl::AudioCapturerImpl()
    :audioSourceLocal_(nullptr),
#ifdef MEDIA_INTERFACE_V1_0
    audioSourceVirtual_(nullptr),
#endif
     audioEncoder_(nullptr)
{
    MEDIA_DEBUG_LOG("ctor");
}

AudioCapturerImpl::~AudioCapturerImpl()
{
    if (status_ != RELEASED) {
        Release();
    }
    MEDIA_ERR_LOG("dtor");
}

bool AudioCapturerImpl::GetMinFrameCount(int32_t sampleRate, int32_t channelCount, AudioCodecFormat audioFormat,
    size_t &frameCount)
{
    return AudioSource::GetMinFrameCount(sampleRate, channelCount, audioFormat, frameCount);
}

uint64_t AudioCapturerImpl::GetFrameCount()
{
    std::shared_ptr<AudioSource> audioSource = GetAudioSource();
    CHK_NULL_RETURN(audioSource, ERROR);
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ == INITIALIZED || status_ == RELEASED) {
        MEDIA_ERR_LOG("check state:%u failed", status_);
        return 0;
    }
    return audioSource->GetFrameCount();
}

State AudioCapturerImpl::GetStatus()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

bool AudioCapturerImpl::GetTimestamp(Timestamp &timestamp, Timestamp::Timebase base)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ == RELEASED) {
        MEDIA_ERR_LOG("check state:%u failed", status_);
        return false;
    }
    timestamp = timestamp_;
    return true;
}

static void FillSourceConfig(AudioSourceConfig &sourceConfig, const AudioCapturerInfo &info, uint32_t deviceId)
{
    sourceConfig.deviceId = deviceId;
    sourceConfig.audioFormat = info.audioFormat;
    sourceConfig.sampleRate = info.sampleRate;
    sourceConfig.channelCount = info.channelCount;
    sourceConfig.interleaved = false;
    sourceConfig.bitWidth = info.bitWidth;
    sourceConfig.streamUsage = info.streamType;
}

static void FillEncConfig(AudioEncodeConfig &encodeConfig, const AudioCapturerInfo &info)
{
    encodeConfig.audioFormat = info.audioFormat;
    encodeConfig.bitRate = info.bitRate;
    encodeConfig.sampleRate = info.sampleRate;
    encodeConfig.channelCount = info.channelCount;
    encodeConfig.bitWidth = info.bitWidth;
}

void AudioCapturerImpl::InitAudioSourceByDeviceType(AudioSystemDeviceType deviceType, std::string deviceId)
{
    if (deviceType == AUDIO_DEVICE_MIC_LOCAL) {
        if (audioSourceLocal_ == nullptr) {
            MEDIA_INFO_LOG("create AudioSourceLocal success!");
            audioSourceLocal_ = std::make_shared<AudioSourceLocal>();
        }
    }
#ifdef MEDIA_INTERFACE_V1_0
    if (deviceType == AUDIO_DEVICE_MIC_VIRTUAL) {
        if (audioSourceVirtual_ == nullptr) {
            MEDIA_INFO_LOG("create AudioSourceVirtual success!");
            audioSourceVirtual_ = std::make_shared<AudioSourceVirtual>(deviceId);
        }
    }
#endif
}

int32_t AudioCapturerImpl::SetCapturerInfo(const OHOS::Audio::AudioCapturerInfo &info)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ != INITIALIZED) {
        MEDIA_ERR_LOG("check state:%u failed", status_);
        return ERR_ILLEGAL_STATE;
    }
    InitAudioSourceByDeviceType(info.deviceType, info.deviceId);
    info_.deviceType = info.deviceType;
    std::shared_ptr<AudioSource> audioSource = GetAudioSource();
    CHK_NULL_RETURN(audioSource, ERROR);
    std::vector<AudioDeviceDesc> devices;
    int32_t ret = audioSource->EnumDeviceBySourceType(info.inputSource, devices);
    if (ret != SUCCESS || devices.empty()) {
        MEDIA_ERR_LOG("EnumDeviceBySourceType failed inputSource:%d", info.inputSource);
        return ret;
    }
    MEDIA_INFO_LOG("info.sampleRate:%d", info.sampleRate);
    AudioSourceConfig sourceConfig;
    FillSourceConfig(sourceConfig, info, devices[0].deviceId);
    ret = audioSource->Initialize(sourceConfig);
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("Initialize failed inputSource:%d", info.inputSource);
        return ret;
    }
    if (info.audioFormat != PCM &&
        info.audioFormat != AUDIO_DEFAULT &&
        info_.deviceType == AUDIO_DEVICE_MIC_LOCAL) {
        AudioEncodeConfig encodeConfig;
        FillEncConfig(encodeConfig, info);
        MEDIA_INFO_LOG("audioEncoder_ bitRate:%d", info.bitRate);
        std::unique_ptr<AudioEncoder> audioEncoder(new(std::nothrow) AudioEncoder());
        audioEncoder_ = std::move(audioEncoder);
        if (audioEncoder_ == nullptr) {
            MEDIA_ERR_LOG("new AudioEncoder failed inputSource:%d", info.inputSource);
            return ERR_UNKNOWN;
        }
        ret = audioEncoder_->Initialize(encodeConfig);
        if (ret != SUCCESS) {
            MEDIA_ERR_LOG("Initialize failed inputSource:%d", info.inputSource);
            (void)audioSource->Release();
            return ret;
        }
    }
    info_ = info;
    status_ = PREPARED;
    MEDIA_INFO_LOG("Set Capturer Info SUCCESS");
    return SUCCESS;
}

int32_t AudioCapturerImpl::GetCapturerInfo(AudioCapturerInfo &info)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ == RELEASED) {
        MEDIA_ERR_LOG("check state:%u failed", status_);
        return ERR_INVALID_OPERATION;
    }
    info = info_;
    return SUCCESS;
}

bool AudioCapturerImpl::Record()
{
    std::shared_ptr<AudioSource> audioSource = GetAudioSource();
    CHK_NULL_RETURN(audioSource, false);
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ != PREPARED && status_ != STOPPED) {
        MEDIA_ERR_LOG("not PREPARED or STOPPED status:%u", status_);
        return false;
    }
    int32_t ret = audioSource->Start();
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("audioSource_ Start failed:0x%x", ret);
        return false;
    }
    if (audioEncoder_ != nullptr) {
        uint32_t deviceId = 0;
        ret = audioSource->GetCurrentDeviceId(deviceId);
        if (ret != SUCCESS) {
            MEDIA_ERR_LOG("audioSource_ GetCurrentDevice failed:0x%x", ret);
            return false;
        }
        inputDeviceId_ = deviceId;
        ret = audioEncoder_->BindSource(deviceId);
        if (ret != SUCCESS) {
            MEDIA_ERR_LOG("audioEncoder_ BindSource failed:0x%x", ret);
            return false;
        }
        ret = audioEncoder_->Start();
        if (ret != SUCCESS) {
            MEDIA_ERR_LOG("audioEncoder_ Start failed:0x%x", ret);
            return false;
        }
    }
    status_ = RECORDING;
    MEDIA_INFO_LOG("Start Audio Capturer SUCCESS");
    return true;
}

int32_t AudioCapturerImpl::Read(uint8_t *buffer, size_t userSize, bool isBlockingRead)
{
    if (buffer == nullptr || !userSize) {
        MEDIA_ERR_LOG("Invalid buffer or userSize:%u", userSize);
        return ERR_INVALID_READ;
    }
    std::shared_ptr<AudioSource> audioSource = GetAudioSource();
    CHK_NULL_RETURN(audioSource, ERROR);

    if (status_ != RECORDING) {
        MEDIA_ERR_LOG("ILLEGAL_STATE  status_:%u", status_);
        return ERR_ILLEGAL_STATE;
    }
    int32_t readLen = ERR_INVALID_READ;
    if (info_.audioFormat == PCM ||
        info_.audioFormat == AUDIO_DEFAULT ||
        info_.deviceType == AUDIO_DEVICE_MIC_VIRTUAL) {
        AudioFrame frame;
        frame.buffer = buffer;
        frame.bufferLen = userSize;
        readLen = audioSource->ReadFrame(frame, isBlockingRead);
        if (readLen == 0) {
            return ERR_INVALID_READ;
        } else if (readLen == ERR_INVALID_READ) {
            MEDIA_ERR_LOG("audioSource_ ReadFrame fail,ret:0x%x", readLen);
            return ERR_INVALID_READ;
        }
        timestamp_.time.tv_sec = frame.time.tvSec;
        timestamp_.time.tv_nsec = frame.time.tvNSec;
    } else {
        AudioStream stream;
        stream.buffer = buffer;
        stream.bufferLen = userSize;

        if (audioEncoder_ == nullptr) {
            MEDIA_ERR_LOG("audioEncoder_ ReadStream fail, audioEncoder_ value is nullptr");
            return ERR_INVALID_READ;
        }
        readLen = audioEncoder_->ReadStream(stream, isBlockingRead);
        if (readLen == ERR_INVALID_READ) {
            MEDIA_ERR_LOG("audioEncoder_ ReadStream fail,ret:0x%x", readLen);
            return ERR_INVALID_READ;
        }
        timestamp_.time.tv_sec = static_cast<time_t>(stream.timeStamp / TIME_CONVERSION_US_S);
        timestamp_.time.tv_nsec = static_cast<time_t>((stream.timeStamp -
            timestamp_.time.tv_sec * TIME_CONVERSION_US_S) * TIME_CONVERSION_NS_US);
    }
    return readLen;
}

bool AudioCapturerImpl::StopInternal()
{
    std::shared_ptr<AudioSource> audioSource = GetAudioSource();
    CHK_NULL_RETURN(audioSource, false);
    int32_t ret;
    if (audioEncoder_ != nullptr) {
        MEDIA_INFO_LOG("audioEncoder Stop");
        ret = audioEncoder_->Stop();
        if (ret != SUCCESS) {
            MEDIA_DEBUG_LOG("audioEncoder_ stop fail,ret:0x%x", ret);
            return false;
        }
    }
    MEDIA_INFO_LOG("audioSource Stop");
    ret = audioSource->Stop();
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("audioSource_ stop fail,ret:0x%x", ret);
        return false;
    }
    MEDIA_INFO_LOG("Stop Audio Capturer SUCCESS");
    status_ = STOPPED;
    return true;
}

std::shared_ptr<AudioSource> AudioCapturerImpl::GetAudioSource()
{
    if (info_.deviceType == AUDIO_DEVICE_MIC_LOCAL) {
        return audioSourceLocal_;
    }
#ifdef MEDIA_INTERFACE_V1_0
    if (info_.deviceType == AUDIO_DEVICE_MIC_VIRTUAL) {
        return audioSourceVirtual_;
    }
#endif
    return nullptr;
}

bool AudioCapturerImpl::Stop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ != RECORDING) {
        MEDIA_ERR_LOG("not RECORDING status:%u", status_);
        return false;
    }
    return StopInternal();
}

bool AudioCapturerImpl::Release()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ == RELEASED) {
        MEDIA_ERR_LOG("ILLEGAL_STATE status_:%u", status_);
        return false;
    }
    if (status_ == INITIALIZED) {
        status_ = RELEASED;
        return true;
    }
    if (status_ == RECORDING) {
        if (!StopInternal()) {
            MEDIA_ERR_LOG("StopInternal err");
            return false;
        }
    }
    int32_t ret;
    if (audioEncoder_ != nullptr) {
        ret = audioEncoder_->Release();
        if (ret != SUCCESS) {
            MEDIA_ERR_LOG("audioEncoder_ Release failed:0x%x", ret);
            return false;
        }
    }
    std::shared_ptr<AudioSource> audioSource = GetAudioSource();
    ret = (audioSource != nullptr) ? audioSource->Release() : SUCCESS;
    if (ret != SUCCESS) {
        MEDIA_ERR_LOG("audioSource_ Release failed:0x%x", ret);
        return false;
    }
    status_ = RELEASED;
    MEDIA_INFO_LOG("Release Audio Capturer SUCCESS");
    return true;
}
void AudioCapturerImpl::SetDeviceChangeCallback(std::shared_ptr<AudioManagerDeviceChangeCallback> callback)
{
#ifdef MEDIA_INTERFACE_V1_0
    std::shared_ptr<AudioManagerDeviceChangeCallbackImpl> callbackImp = \
        std::make_shared<AudioManagerDeviceChangeCallbackImpl>(callback);
    OHOS::HDI::DistributedAudio::Audio::V1_0::AudioManagerInterfaceImpl::GetAudioManager() \
        ->SetDeviceChangeCallback(callbackImp);
#endif
    MEDIA_INFO_LOG("set AudioSourceVirtual callback success!");
}
}  // namespace Audio
}  // namespace OHOS
