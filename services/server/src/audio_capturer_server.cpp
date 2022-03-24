/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
        IpcIoPushInt32(reply, MEDIA_OK);
    } else {
        IpcIoPushInt32(reply, MEDIA_IPC_FAILED);
    }
}

void AudioCapturerServer::ReadAudioDataProcessExit(void)
{
    MEDIA_INFO_LOG("ReadAudioDataProcessExit");
    if (dataThreadId_) {
        threadExit_ = true;
        pthread_join(dataThreadId_, nullptr);
        threadExit_ = false;
        dataThreadId_ = 0;
    }
}

void AudioCapturerServer::DropServer(pid_t pid, IpcIo *reply)
{
    MEDIA_INFO_LOG("in");
    if (pid == clientPid_) {
        ReadAudioDataProcessExit();
        delete capturer_;
        capturer_ = nullptr;
        clientPid_ = -1;
    }
    if (surface_ != nullptr) {
        delete surface_;
        surface_ = nullptr;
    }
    IpcIoPushInt32(reply, MEDIA_OK);
}

void *AudioCapturerServer::ReadAudioDataProcess(void *serverStr)
{
    AudioCapturerServer *serverStore = reinterpret_cast<AudioCapturerServer*>(serverStr);
    if (serverStore == nullptr || serverStore->surface_ == nullptr) {
        MEDIA_ERR_LOG("No available serverStore in surface.");
        return nullptr;
    }

    MEDIA_INFO_LOG("thread work");
    while (!serverStore->threadExit_) {
        /* request surface buffer */
        SurfaceBuffer *surfaceBuf = serverStore->surface_->RequestBuffer();
        if (surfaceBuf == nullptr) {
            usleep(20000); // indicates 20000 microseconds
            continue;
        }
        uint32_t size = serverStore->surface_->GetSize();
        void *buf = surfaceBuf->GetVirAddr();
        uint32_t offSet = sizeof(Timestamp);
        /* if offset more then size , will lead to overflow */
        if (offSet > size) {
            MEDIA_ERR_LOG("offSet more then serverStore , can be overflow");
            continue;
        }
        /* Timestamp + audio data */
        /* read frame data, and reserve timestamp space */
        int32_t readLen = serverStore->capturer_->Read(reinterpret_cast<uint8_t*>(buf) + offSet, size - offSet, true);
        if (readLen == ERR_INVALID_READ) {
            serverStore->surface_->CancelBuffer(surfaceBuf);
            usleep(20000); // indicates 20000 microseconds
            continue;
        }

        Timestamp timestamp;
        Timestamp::Timebase base = {};
        bool ret =  serverStore->capturer_->GetTimestamp(timestamp, base);
        if (!ret) {
            MEDIA_ERR_LOG("No readtime get.");
            serverStore->surface_->CancelBuffer(surfaceBuf);
            continue;
        }
        (void)memcpy_s(reinterpret_cast<uint8_t*>(buf), sizeof(Timestamp), &timestamp, sizeof(Timestamp));
        surfaceBuf->SetSize(sizeof(Timestamp) + readLen);

        // flush buffer
        if (serverStore->surface_->FlushBuffer(surfaceBuf)) {
            MEDIA_ERR_LOG("Flush surface buffer failed.");
            serverStore->surface_->CancelBuffer(surfaceBuf);
            ret = MEDIA_ERR;
            continue;
        }
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

void AudioCapturerServer::SetInfo(AudioCapturerImpl *capturer, IpcIo *req, IpcIo *reply)
{
    AudioCapturerInfo info;
    uint32_t size = 0;
    void *bufferAdd = IpcIoPopFlatObj(req, &size);

    if (bufferAdd == nullptr || !size) {
        MEDIA_INFO_LOG("IpcIoPopFlatObj info failed");
        IpcIoPushInt32(reply, -1);
        return;
    }
    (void)memcpy_s(&info, sizeof(AudioCapturerInfo), bufferAdd, size);
    int32_t ret = capturer->SetCapturerInfo(info);
    IpcIoPushInt32(reply, ret);
}

void AudioCapturerServer::GetInfo(AudioCapturerImpl *capturer, IpcIo *reply)
{
    if (capturer == nullptr) {
        MEDIA_ERR_LOG("GetInfo faild, capturer value is nullptr");
        return;
    }

    AudioCapturerInfo info;
    int32_t ret = capturer->GetCapturerInfo(info);
    IpcIoPushInt32(reply, ret);
    IpcIoPushFlatObj(reply, &info, sizeof(AudioCapturerInfo));
}

void AudioCapturerServer::Start(AudioCapturerImpl *capturer, IpcIo *reply)
{
    if (capturer == nullptr) {
        MEDIA_ERR_LOG("Start faild, capturer value is nullptr");
        return;
    }

    bool record = capturer->Record();
    if (record) {
        threadExit_ = false;
        pthread_create(&dataThreadId_, nullptr, ReadAudioDataProcess, this);
        MEDIA_INFO_LOG("create thread ReadAudioDataProcess SUCCESS");
    }
    IpcIoPushInt32(reply, record);
}

void AudioCapturerServer::Stop(AudioCapturerImpl *capturer, IpcIo *reply)
{
    if (capturer == nullptr) {
        MEDIA_ERR_LOG("Stop faild, capturer value is nullptr");
        return;
    }

    int32_t ret = capturer->Stop();
    ReadAudioDataProcessExit();
    IpcIoPushInt32(reply, ret);
}

void AudioCapturerServer::GetMinFrameCount(IpcIo *req, IpcIo *reply)
{
    if (reply == nullptr) {
        MEDIA_ERR_LOG("GetMinFrameCount faild, reply value is nullptr");
        return;
    }

    uint32_t size = 0;
    int32_t sampleRate = IpcIoPopInt32(req);
    int32_t channelCount = IpcIoPopInt32(req);
    AudioCodecFormat *audioFormat = (AudioCodecFormat *)IpcIoPopFlatObj(req, &size);

    size_t frameCount;
    bool ret = AudioCapturerImpl::GetMinFrameCount(sampleRate, channelCount, *audioFormat, frameCount);
    IpcIoPushInt32(reply, ret);
    IpcIoPushUint64(reply, frameCount);
}

void AudioCapturerServer::GetFrameCount(AudioCapturerImpl *capturer, IpcIo *reply)
{
    if (capturer == nullptr) {
        MEDIA_ERR_LOG("GetFrameCount faild, capturer value is nullptr");
        return;
    }

    uint64_t frameCount = capturer->GetFrameCount();
    IpcIoPushInt32(reply, MEDIA_OK);
    IpcIoPushUint64(reply, frameCount);
}

void AudioCapturerServer::GetStatus(AudioCapturerImpl *capturer, IpcIo *reply)
{
    if (capturer == nullptr) {
        MEDIA_ERR_LOG("GetStatus faild, capturer value is nullptr");
        return;
    }

    State status = capturer->GetStatus();
    IpcIoPushInt32(reply, MEDIA_OK);
    IpcIoPushInt32(reply, status);
}

void AudioCapturerServer::SetSurface(IpcIo *req, IpcIo *reply)
{
    Surface *surface = SurfaceImpl::GenericSurfaceByIpcIo(*req);
    if (surface == nullptr) {
        MEDIA_ERR_LOG("SetSurface faild, surface value is nullptr");
        return;
    }
    int32_t ret = SetSurfaceProcess(surface);
    IpcIoPushInt32(reply, ret);
}

void AudioCapturerServer::Dispatch(int32_t funcId, pid_t pid, IpcIo *req, IpcIo *reply)
{
    int32_t ret;
    if (funcId == AUD_CAP_FUNC_GET_MIN_FRAME_COUNT) {
        GetMinFrameCount(req, reply);
        return;
    }
    if (funcId == AUD_CAP_FUNC_CONNECT) {
        AcceptServer(pid, reply);
        return;
    }
    auto capturer = GetAudioCapturer(pid);
    if (capturer == nullptr) {
        MEDIA_ERR_LOG("Cannot find client object.(pid=%d)", pid);
        IpcIoPushInt32(reply, MEDIA_IPC_FAILED);
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
            ReadAudioDataProcessExit();
            IpcIoPushInt32(reply, ret);
            break;
        case AUD_CAP_FUNC_SET_SURFACE:
            SetSurface(req, reply);
            break;
        default:
            break;
    }
}
}  // namespace Audio
}  // namespace OHOS
