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

#include "audio_device_manager.h"
#include "media_log.h"

namespace OHOS {
    namespace Audio {
        AudioDeviceManager::AudioDeviceManager()
        {
        }
            
        AudioDeviceManager::~AudioDeviceManager()
        {
        }

        AudioDeviceManager *AudioDeviceManager::GetInstance()
        {
            static AudioDeviceManager audioManager;
            return &audioManager;
        }

        int32_t AudioDeviceManager::AddDevice(const AudioDeviceInfo &deviceInfo)
        {
            std::lock_guard<std::mutex> guard(mutex_);
            bool isFound = false;
            for (size_t i = 0; i < allDevices_.size(); i++) {
                if (allDevices_[i].dhId == deviceInfo.dhId) {
                    allDevices_[i].connectStatus = deviceInfo.connectStatus;
                    isFound = true;
                    break;
                }
            }
            if (isFound) {
                MEDIA_INFO_LOG("The device[%u] already exists and does not need to be added!", deviceInfo.dhId);
                return MEDIA_OK;
            }
            allDevices_.push_back(deviceInfo);
            return MEDIA_OK;
        }

        int32_t AudioDeviceManager::RemoveDevice(const AudioDeviceInfo &deviceInfo)
        {
            std::lock_guard<std::mutex> guard(mutex_);
            bool isFound = false;
            for (auto it = allDevices_.begin(); it != allDevices_.end(); it++) {
                if ((*it).dhId == deviceInfo.dhId) {
                    (*it).connectStatus = deviceInfo.connectStatus;
                    isFound = true;
                    break;
                }
            }
            if (!isFound) {
                MEDIA_INFO_LOG("The device[%u] does not exist and cannot be deleted.!", deviceInfo.dhId);
                return MEDIA_OK;
            }
            return MEDIA_OK;
        }

        std::vector<AudioDeviceInfo> AudioDeviceManager::GetAllConnectDevices()
        {
            std::lock_guard<std::mutex> guard(mutex_);
            std::vector<AudioDeviceInfo> info;
            for (size_t i = 0; i < allDevices_.size(); i++) {
                if (allDevices_[i].connectStatus == CONNECT) {
                    info.push_back(allDevices_[i]);
                }
            }
            return info;
        }
    }
}