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

#ifndef FRAMEWORKS_AUDIO_CAPTURER_IMPL_H
#define FRAMEWORKS_AUDIO_CAPTURER_IMPL_H

#include <mutex>
#include <sys/time.h>
#include "audio_capturer.h"
#include "media_info.h"

namespace OHOS {
namespace Audio {
enum AudioChannel {
    AUDIO_CHANNEL_IN_MONO   = 1, /* mono */
    AUDIO_CHANNEL_IN_STEREO = 2, /* stereo */
    AUDIO_CHANNEL_BUTT
};

class AudioSource;
class AudioEncoder;
class AudioSourceLocal;
class AudioSourceVirtual;
class AudioCapturerImpl {
public:
    AudioCapturerImpl();
    virtual ~AudioCapturerImpl();
    static bool GetMinFrameCount(int32_t sampleRate, int32_t channelCount, AudioCodecFormat audioFormat,
                                 size_t &frameCount);
    int32_t SetCapturerInfo(const OHOS::Audio::AudioCapturerInfo &info);
    int32_t GetCapturerInfo(OHOS::Audio::AudioCapturerInfo &info);
    bool Record();
    bool Stop();
    bool Release();
    int32_t Read(uint8_t *buffer, size_t userSize, bool isBlockingRead);
    uint64_t GetFrameCount();
    State GetStatus();
    bool GetTimestamp(Timestamp &timestamp, Timestamp::Timebase base);
    void SetDeviceChangeCallback(std::shared_ptr<AudioManagerDeviceChangeCallback> callback);
private:
    bool StopInternal();
    void InitAudioSourceByDeviceType(AudioSystemDeviceType deviceType, std::string deviceId);
    std::shared_ptr<AudioSource> GetAudioSource();
    std::shared_ptr<AudioSourceLocal> audioSourceLocal_;
    std::shared_ptr<AudioSourceVirtual> audioSourceVirtual_;
    std::unique_ptr<AudioEncoder> audioEncoder_;
    State status_ = INITIALIZED;
    OHOS::Audio::AudioCapturerInfo info_;
    Timestamp timestamp_;
    int32_t inputDeviceId_ = 0;
    std::mutex mutex_;
};
}  // namespace Audio
}  // namespace OHOS
#endif  // FRAMEWORKS_AUDIO_CAPTURER_IMPL_H
