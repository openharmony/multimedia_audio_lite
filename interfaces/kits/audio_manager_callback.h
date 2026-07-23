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

#ifndef AUDIO_MANAGER_CALL_BACK_H
#define AUDIO_MANAGER_CALL_BACK_H

#include "media_info.h"
namespace OHOS {
    namespace Audio {
        class AudioManagerDeviceChangeCallback {
            public:
            virtual ~AudioManagerDeviceChangeCallback() = default;
            /**
             * Called when an interrupt is received.
             *
             * @param deviceChangeAction Indicates the DeviceChangeAction information needed by client.
             * For details, refer DeviceChangeAction struct
             * @since 8
             */
            virtual void OnDeviceChange(const AudioDeviceInfo &deviceInfo) = 0;
            virtual void OnReadDataFailed() = 0;
        };

        typedef enum {
            ON_DEVICE_CHANGED,
            ON_READ_DATA_FAILED,
        } AudioCapturerClientCall;
    }
}

#endif