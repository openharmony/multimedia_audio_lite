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
#ifndef AUDIO_SYSTEM_MANAGER_H
#define AUDIO_SYSTEM_MANAGER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <mutex>
#include "media_errors.h"
#include "media_info.h"

namespace OHOS {
namespace Audio {
    class AudioDeviceManager {
    public:
        ~AudioDeviceManager();
        static AudioDeviceManager *GetInstance();
        std::vector<AudioDeviceInfo> GetAllConnectDevices();
        int32_t AddDevice(const AudioDeviceInfo &deviceinfo);
        int32_t RemoveDevice(const AudioDeviceInfo &deviceinfo);
    private:
        AudioDeviceManager();
        std::vector<AudioDeviceInfo> allDevices_;
        std::mutex mutex_;
    };
}
}
#endif