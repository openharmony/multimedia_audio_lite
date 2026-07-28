/*
 * Copyright (c) 2026 HiSilicon (Shanghai) Technologies Co., Ltd.
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

#ifndef FRAMEWORKS_AUDIO_SOURCE_INCLUDE_AUDIO_VIRTUAL_H_
#define FRAMEWORKS_AUDIO_SOURCE_INCLUDE_AUDIO_VIRTUAL_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <time.h>
#include <vector>

#include "audio_source.h"
#include "iaudio_capture.h"
#include "iaudio_adapter.h"
#include "iaudio_manager.h"
#include "audio_manager_callback.h"
#include "iaudio_device_callback.h"

namespace OHOS {
namespace Audio {
using namespace OHOS::HDI::DistributedAudio::Audio::V1_0;
class AudioManagerDeviceChangeCallbackImpl : public IAudioManagerDeviceChangeCallback {
public:
    AudioManagerDeviceChangeCallbackImpl(std::shared_ptr<AudioManagerDeviceChangeCallback> callback)
        : callback_(callback) {}
    ~AudioManagerDeviceChangeCallbackImpl() override {}
    void OnDeviceChange(const AudioDeviceInfo &deviceInfo) override
    {
        if (callback_ != nullptr) {
            callback_->OnDeviceChange(deviceInfo);
        }
    }
private:
    std::shared_ptr <AudioManagerDeviceChangeCallback> callback_ = nullptr;
};

class AudioSourceVirtual : public AudioSource {
public:
    AudioSourceVirtual(std::string deviceId);
    ~AudioSourceVirtual();

    /**
     * Enumerates currently supported devices by audio source type.
     *
     * @param inputSource the type of source audio.
     * @param devices holds an array of satisfied audio device description, including name and identity.
     * @return Returns SUCCESS if success, other values otherwise.
     */
    int32_t EnumDeviceBySourceType(AudioSourceType inputSource, std::vector<AudioDeviceDesc> &devices);

    /**
     * Obtains the frame count (in BytesPerSample) required in the current conditions.
     *
     * @return Returns the frame count (in BytesPerSample); returns {@code -1} if an exception occurs.
     */
    uint64_t GetFrameCount();

    /**
     * Initializes the audio source according to a specific configuration.
     *
     * @param config a configuration of audio source.
     * @return Returns SUCCESS if success, other values otherwise.
     */
    int32_t Initialize(const AudioSourceConfig &config);

    /**
     * Sets input device's identity when switching device.
     *
     * @param deviceId identity to set.
     * @return Returns SUCCESS if set successfully, other values otherwise.
    */
    int32_t SetInputDevice(uint32_t deviceId);

    /**
     * Gets current device's identity.
     *
     * @param deviceId holds the identity of current device, if success.
     * @return Returns SUCCESS if success, other values otherwise.
     */
    int32_t GetCurrentDeviceId(uint32_t &deviceId);

    /**
     * Starts audio source.
     *
     * @return Returns SUCCESS if success, other values otherwise.
    */
    int32_t Start();

    /**
     *
     * Reads frame from source.
     *
     * @param frame, the buffer to storage the info of frame that read from source.
     * @param isBlockingRead reading mode.
     * @return Returns size of data actually read.
    */
    int32_t ReadFrame(AudioFrame &frame, bool isBlockingRead);

    /**
     * Stops audio source.
     *
     * @return Returns SUCCESS if success, other values otherwise.
    */
    int32_t Stop();

    /**
    * release.
    */
    int32_t Release();

    void SetDeviceChangeCallback(const std::shared_ptr<IAudioManagerDeviceChangeCallback> &callback);

private:
    int32_t InitCheck();
    void AudioAdapterDescriptorMatch(const AudioAdapterDescriptor &desc);
    IAudioManager *audioManager_ = nullptr;
    bool initialized_;
    bool started_;
    std::string deviceId_;
    std::string adapterName_;
    std::shared_ptr<IAudioAdapter> audioAdapter_;
    std::shared_ptr<IAudioCapture> audioCapture_;
    std::vector<OHOS::HDI::DistributedAudio::Audio::V1_0::AudioPort> capturePort_ = {};
};
}  // namespace Audio
}  // namespace OHOS
#endif  // FRAMEWORKS_AUDIO_SOURCE_INCLUDE_AUDIO_SOURCE_H_
