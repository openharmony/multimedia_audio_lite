/*
 * Copyright (c) 2020 Huawei Device Co., Ltd.
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

#include "audio_capturer_server.h"
#include "audio_capturer_impl.h"
#include "media_errors.h"
#include "media_log.h"
#include "securec.h"
#include "surface.h"
#include "surface_impl.h"

using namespace std;
using namespace OHOS::Media;
namespace OHOS {
namespace Audio {
AudioCapturerServer *AudioCapturerServer::GetInstance()
{
    static AudioCapturerServer mng;
    return &mng;
}

AudioCapturerImpl *AudioCapturerServer::GetAudioCapturer(pid_t pid)
{
    return (pid == clientPid_) ? capturer_ : nullptr;
}

int32_t AudioCapturerServer::AudioCapturerServerInit()
{
    return 0;
}

void AudioCapturerServer::AcceptServer(pid_t pid, IpcIo *reply)
{
    MEDIA_INFO_LOG("in");
    if (clientPid_ == -1) {
        capturer_ = new AudioCapturerImpl;
        clientPid_ = pid;
        WriteInt32(reply, MEDIA_OK);
    } else {
        WriteInt32(reply, MEDIA_IPC_FAILED);
    }
}

void AudioCapturerServer::DropServer(pid_t pid, IpcIo *reply)
{
    MEDIA_INFO_LOG("in");
    if (pid == clientPid_) {
        if (dataThreadId_ != 0) {
            threadExit_ = true;
            pthread_join(dataThreadId_, nullptr);
            threadExit_ = false;
            dataThreadId_ = 0;
        }
        delete capturer_;
        capturer_ = nullptr;
        clientPid_ = -1;
        bufCache_ = nullptr;
    }
    WriteInt32(reply, MEDIA_OK);
}

SurfaceBuffer *AudioCapturerServer::GetCacheBuffer(void)
{
    if (surface_ == nullptr) {
        MEDIA_ERR_LOG("No available serverStore in surface.");
        return nullptr;
    }

    if (bufCache_ == nullptr) {
        bufCache_ = surface_->RequestBuffer();
    }
    return bufCache_;
}

void AudioCapturerServer::CancelBuffer(SurfaceBuffer *buffer)
{
    surface_->CancelBuffer(buffer);
    FreeCacheBuffer();
}

void AudioCapturerServer::FreeCacheBuffer(void)
{
    bufCache_ = nullptr;
}

void *AudioCapturerServer::ReadAudioDataProcess(void *serverStr)
{
    AudioCapturerServer *serverStore = (AudioCapturerServer *)serverStr;
    if (serverStore == nullptr || serverStore->surface_ == nullptr) {
        MEDIA_ERR_LOG("No available serverStore in surface.");
        return nullptr;
    }

    MEDIA_INFO_LOG("thread work");
    while (!serverStore->threadExit_) {
        /* request surface buffer */
        SurfaceBuffer *surfaceBuf = serverStore->GetCacheBuffer();
        if (surfaceBuf == nullptr) {
            usleep(5000); // indicates 5000 microseconds
            continue;
        }
        uint32_t size = serverStore->surface_->GetSize();
        void *buf = surfaceBuf->GetVirAddr();
        if (buf == nullptr) {
            serverStore->CancelBuffer(surfaceBuf);
            continue;
        }
        uint32_t offSet = sizeof(Timestamp);
        /* Timestamp + audio data */
        /* read frame data, and reserve timestamp space */
        int32_t readLen = serverStore->capturer_->Read((uint8_t *)buf + offSet, size - offSet, true);
        if (readLen == ERR_INVALID_READ) {
            continue;
        }

        Timestamp timestamp;
        Timestamp::Timebase base = {};
        bool ret =  serverStore->capturer_->GetTimestamp(timestamp, base);
        if (!ret) {
            MEDIA_ERR_LOG("No readtime get.");
            continue;
        }
        errno_t retCopy = memcpy_s((uint8_t *)buf, sizeof(Timestamp), &timestamp, sizeof(Timestamp));
        if (retCopy != EOK) {
            MEDIA_ERR_LOG("retCopy = %x", retCopy);
            continue;
        }
        surfaceBuf->SetSize(sizeof(Timestamp) + readLen);

        // flush buffer
        if (serverStore->surface_->FlushBuffer(surfaceBuf) != 0) {
            MEDIA_ERR_LOG("Flush surface buffer failed.");
            serverStore->CancelBuffer(surfaceBuf);
            ret = MEDIA_ERR;
            continue;
        }
        serverStore->FreeCacheBuffer();
    }
    MEDIA_INFO_LOG("thread exit");
    return nullptr;
}

int32_t AudioCapturerServer::SetSurfaceProcess(Surface *surface)
{
    if (surface == nullptr) {
        MEDIA_INFO_LOG("Surface is null");
        return -1;
    }
    surface_ = surface;

    return 0;
}

void AudioCapturerServer::GetMinFrameCount(IpcIo *req, IpcIo *reply)
{
    int32_t sampleRate = 0;
    ReadInt32(req, &sampleRate);
    int32_t channelCount = 0;
    ReadInt32(req, &channelCount);
    int32_t data = 0;
    ReadInt32(req, &data);
    AudioCodecFormat audioFormat = (AudioCodecFormat)data;
    size_t frameCount;
    bool ret = AudioCapturerImpl::GetMinFrameCount(sampleRate, channelCount, audioFormat, frameCount);
    WriteInt32(reply, ret);
    WriteUint32(reply, frameCount);
}

void AudioCapturerServer::SetInfo(AudioCapturerImpl *capturer, IpcIo *req, IpcIo *reply)
{
    AudioCapturerInfo info;
    uint32_t size = 0;
    ReadUint32(req, &size);
    void *bufferAdd = (void*)ReadBuffer(req, size);

    if (bufferAdd == nullptr || size == 0) {
        MEDIA_INFO_LOG("Readbuffer info failed");
        WriteInt32(reply, -1);
        return;
    }
    errno_t retCopy = memcpy_s(&info, sizeof(AudioCapturerInfo), bufferAdd, size);
    if (retCopy != EOK) {
        MEDIA_ERR_LOG("retCopy = %x", retCopy);
        return;
    }
    int32_t ret = capturer->SetCapturerInfo(info);
    WriteInt32(reply, ret);
}

void AudioCapturerServer::GetInfo(AudioCapturerImpl *capturer, IpcIo *reply)
{
    if (capturer == nullptr) {
        MEDIA_ERR_LOG("GetInfo failed, capturer value is nullptr");
        return;
    }
    
    AudioCapturerInfo info;
    int32_t ret = capturer->GetCapturerInfo(info);
    WriteInt32(reply, ret);
    WriteUint32(reply, sizeof(AudioCapturerInfo));
    WriteBuffer(reply, &info, sizeof(AudioCapturerInfo));
}

void AudioCapturerServer::Start(AudioCapturerImpl *capturer, IpcIo *reply)
{
    if (capturer == nullptr) {
        MEDIA_ERR_LOG("Start failed, capturer value is nullptr");
        return;
    }

    bool record = capturer->Record();
    if (record) {
        threadExit_ = false;
        pthread_create(&dataThreadId_, nullptr, ReadAudioDataProcess, this);
        MEDIA_INFO_LOG("create thread ReadAudioDataProcess SUCCESS");
    }
    WriteInt32(reply, record);
}

void AudioCapturerServer::Stop(AudioCapturerImpl *capturer, IpcIo *reply)
{
    if (capturer == nullptr) {
        MEDIA_ERR_LOG("Stop failed, capturer value is nullptr");
        return;
    }
    int32_t ret = capturer->Stop();
    if (dataThreadId_ != 0) {
        threadExit_ = true;
        pthread_join(dataThreadId_, nullptr);
        threadExit_ = false;
        dataThreadId_ = 0;
    }
    WriteInt32(reply, ret);
}

void AudioCapturerServer::GetMiniFrameCount(IpcIo *req, IpcIo *reply)
{
    if (reply == nullptr) {
        MEDIA_ERR_LOG("GetMinFrameCount failed, reply value is nullptr");
        return;
    }

    int32_t sampleRate = 0;
    ReadInt32(req, &sampleRate);
    int32_t channelCount = 0;
    ReadInt32(req, &channelCount);
    uint32_t size = 0;
    ReadUint32(req, &size);
    AudioCodecFormat *audioFormat = (AudioCodecFormat *)ReadBuffer(req, size);

    size_t frameCount;
    bool ret = AudioCapturerImpl::GetMinFrameCount(sampleRate, channelCount, *audioFormat, frameCount);
    WriteInt32(reply, ret);
    WriteUint64(reply, frameCount);
}

void AudioCapturerServer::GetFrameCount(AudioCapturerImpl *capturer, IpcIo *reply)
{
    if (capturer == nullptr) {
        MEDIA_ERR_LOG("GetFrameCount failed, capturer value is nullptr");
        return;
    }

    uint64_t frameCount = capturer->GetFrameCount();
    WriteInt32(reply, MEDIA_OK);
    WriteUint64(reply, frameCount);
}

void AudioCapturerServer::GetStatus(AudioCapturerImpl *capturer, IpcIo *reply)
{
    if (capturer == nullptr) {
        MEDIA_ERR_LOG("GetStatus failed, capturer value is nullptr");
        return;
    }

    State status = capturer->GetStatus();
    WriteInt32(reply, MEDIA_OK);
    WriteInt32(reply, status);
}

void AudioCapturerServer::SetSurface(IpcIo *req, IpcIo *reply)
{
    Surface *surface = SurfaceImpl::GenericSurfaceByIpcIo(*req);
    if (surface == nullptr) {
        MEDIA_ERR_LOG("SetSurface failed, surface value is nullptr");
        return;
    }
    int32_t ret = SetSurfaceProcess(surface);
    WriteInt32(reply, ret);
}

void AudioCapturerServer::Dispatch(int32_t funcId, pid_t pid, IpcIo *req, IpcIo *reply)
{
    int32_t ret;
    if (funcId == AUD_CAP_FUNC_GET_MIN_FRAME_COUNT) {
        return;
    }
    if (funcId == AUD_CAP_FUNC_CONNECT) {
        AcceptServer(pid, reply);
        return;
    }
    auto capturer = GetAudioCapturer(pid);
    if (capturer == nullptr) {
        MEDIA_ERR_LOG("Cannot find client object.(pid=%d)", pid);
        WriteInt32(reply, MEDIA_IPC_FAILED);
        return;
    }
    switch (funcId) {
        case AUD_CAP_FUNC_DISCONNECT:
            DropServer(pid, reply);
            break;
        case AUD_CAP_FUNC_GET_FRAME_COUNT:
            GetFrameCount(capturer, reply);
            break;
        case AUD_CAP_FUNC_GET_STATUS:
            GetStatus(capturer, reply);
            break;
        case AUD_CAP_FUNC_SET_INFO:
            SetInfo(capturer, req, reply);
            break;
        case AUD_CAP_FUNC_GET_INFO:
            GetInfo(capturer, reply);
            break;
        case AUD_CAP_FUNC_START:
            Start(capturer, reply);
            break;
        case AUD_CAP_FUNC_STOP:
            Stop(capturer, reply);
            break;
        case AUD_CAP_FUNC_RELEASE:
            ret = capturer->Release();
            WriteInt32(reply, ret);
            break;
        case AUD_CAP_FUNC_SET_SURFACE:
            SetSurface(req, reply);
            break;
        case AUD_CAP_FUNC_GET_MIN_FRAME_COUNT:
            GetMiniFrameCount(req, reply);
            break;
        default:
            break;
    }
}
}  // namespace Audio
}  // namespace OHOS
