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

#include "audio_source.h"
#include "media_log.h"
#include "securec.h"

namespace OHOS {
namespace Audio {

AudioSource::AudioSource()
{
}

AudioSource::~AudioSource()
{
}


bool AudioSource::GetMinFrameCount(int32_t sampleRate, int32_t channelCount,
                                   AudioCodecFormat audioFormat, size_t &frameCount)
{
    if (sampleRate <= 0 || channelCount <= 0 || audioFormat < AUDIO_DEFAULT ||
        audioFormat >= FORMAT_BUTT) {
        MEDIA_ERR_LOG("invalid params sampleRate:%d channelCount:%d audioFormat:%d", sampleRate,
                      channelCount, audioFormat);
        return false;
    }
    frameCount = 0;
    return true;
}
}  // namespace Audio
}  // namespace OHOS
