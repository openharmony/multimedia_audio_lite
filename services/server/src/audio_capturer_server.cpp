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
#include <sstream>
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

constexpr int32_t WAIT_SURFACE_BUFFER_US = 10000;
constexpr int32_t WAIT_READ_SORFTBUS_DATA_US = 10000;

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
    if (capturer_ == nullptr && clientPid_ == -1) {
        capturer_ = new AudioCapturerImpl;
    }
    callback_ = std::make_shared<AudioCapturerServerCallback>();
    if (capturer_ != nullptr) {
        capturer_->SetDeviceChangeCallback(callback_);
    }
    return 0;
}

void AudioCapturerServer::AcceptServer(pid_t pid, IpcIo *reply)
{
    MEDIA_INFO_LOG("in");
    if (clientPid_ == -1) {
        if (capturer_ == nullptr) {
            capturer_ = new AudioCapturerImpl;
            callback_ = std::make_shared<AudioCapturerServerCallback>();
            if (capturer_ != nullptr) {
                capturer_->SetDeviceChangeCallback(callback_);
            }
        }
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
        callback_->SetCanCallback(false);
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

void AudioCapturerServer::SetAudioCapturerServerCallback(AudioCapturerImpl *capturer, IpcIo *req)
{
    SvcIdentity sid;
    if (ReadRemoteObject(req, &sid)) {
        if (callback_ != nullptr) {
            callback_->SetSvcIdentity(sid);
            callback_->SetCanCallback(true);
            OnAllDeviceChange();
        }
    }
}

void AudioCapturerServerCallback::SetSvcIdentity(SvcIdentity sid)
{
    sid_ = sid;
}

void AudioCapturerServerCallback::SetCanCallback(bool isCallback)
{
    isCallback_.store(isCallback);
}

SvcIdentity AudioCapturerServerCallback::GetSvcIdentity()
{
    return sid_;
}

void AudioCapturerServerCallback::OnDeviceChange(const AudioDeviceInfo &deviceInfo)
{
    MEDIA_INFO_LOG("revice Device changed,device dhid = %d", deviceInfo.dhId);
    AudioCapturerServer::GetInstance()->addDeviceInfo(deviceInfo);
    if (isCallback_.load()) {
        AudioCapturerServer::GetInstance()->SendDeviceInfo(deviceInfo);
    }
}

void AudioCapturerServerCallback::OnReadDataFailed(void)
{
    MEDIA_INFO_LOG("revice Read data failed event");
    if (isCallback_.load()) {
        AudioCapturerServer::GetInstance()->SendReadDataFailInfo();
    }
}

int32_t AudioCapturerServer::SendDeviceInfo(const AudioDeviceInfo &deviceInfo)
{
    IpcIo io;
    uint8_t tmpData[DEFAULT_IPC_SIZE];
    int32_t size = 0;
    IpcIoInit(&io, tmpData, DEFAULT_IPC_SIZE, size);
    WriteUint32(&io, (int32_t)deviceInfo.dhId);
    WriteUint32(&io, deviceInfo.deviceName.size());
    WriteBuffer(&io, deviceInfo.deviceName.c_str(), deviceInfo.deviceName.size());
    WriteInt32(&io, (int32_t)deviceInfo.deviceType);
    WriteInt32(&io, (int32_t)deviceInfo.connectStatus);
    MessageOption option;
    MessageOptionInit(&option);
    option.flags = TF_OP_ASYNC;
    int32_t ret = SendRequest(callback_->GetSvcIdentity(), ON_DEVICE_CHANGED, &io, nullptr, option, nullptr);
    if (ret != MEDIA_OK) {
        MEDIA_ERR_LOG("AudioCapturerServerCallback::OnDeviceChange failed,deviceInfo.dhid = %d\n", deviceInfo.dhId);
        return -1;
    }
    return 0;
}

void AudioCapturerServer::SendReadDataFailInfo(void)
{
    if (callback_ == nullptr) {
        MEDIA_ERR_LOG("AudioCapturerServerCallback::ON_READ_DATA_FAILED failed");
        return;
    }
    IpcIo io;
    uint8_t tmpData[DEFAULT_IPC_SIZE];
    int32_t size = 0;
    IpcIoInit(&io, tmpData, DEFAULT_IPC_SIZE, size);
    MessageOption option;
    MessageOptionInit(&option);
    option.flags = TF_OP_ASYNC;
    int32_t ret = SendRequest(callback_->GetSvcIdentity(), ON_READ_DATA_FAILED, &io, nullptr, option, nullptr);
    if (ret != MEDIA_OK) {
        MEDIA_ERR_LOG("AudioCapturerServerCallback::ON_READ_DATA_FAILED failed");
    }
}

void AudioCapturerServer::addDeviceInfo(AudioDeviceInfo deviceInfo)
{
    std::lock_guard<std::mutex> lck(callbackLock_);
    std::vector<AudioDeviceInfo>::iterator it = deviceInfoList_.begin();
    bool isFound = false;
    for (; it != deviceInfoList_.end(); it++) {
        AudioDeviceInfo newDeviceInfo = (*it);
        if (deviceInfo.dhId == newDeviceInfo.dhId) {
            newDeviceInfo.connectStatus = deviceInfo.connectStatus;
            isFound = true;
        }
    }
    if (!isFound) {
        deviceInfoList_.push_back(deviceInfo);
    }
}

void AudioCapturerServer::OnAllDeviceChange()
{
    std::lock_guard<std::mutex> lck(callbackLock_);
    std::vector<AudioDeviceInfo>::iterator it = deviceInfoList_.begin();
    for (; it != deviceInfoList_.end(); it++) {
        AudioDeviceInfo newDeviceInfo = (*it);
        int32_t ret = SendDeviceInfo(newDeviceInfo);
        if (ret != 0) {
            break;
        }
    }
}

bool AudioCapturerServer::ReadAudioBuffer(AudioCapturerServer *serverStore, \
    SurfaceBuffer *surfaceBuf, void *buf, uint32_t size)
{
    if (serverStore == nullptr || serverStore->capturer_ == nullptr) {
        MEDIA_ERR_LOG("No available serverStore in capturer.");
        return false;
    }
    uint32_t offSet = sizeof(Timestamp);
    int32_t readLen = serverStore->capturer_->Read((uint8_t *)buf + offSet, size - offSet, true);
    if (readLen == ERR_INVALID_READ) {
        usleep(WAIT_READ_SORFTBUS_DATA_US);
        return false;
    } else if (readLen == ERR_ILLEGAL_STATE ||
        readLen == ERR_AUDIO_READ_DATA_TIME_OUT) {
        serverStore->SendReadDataFailInfo();
        return false;
    }
    Timestamp timestamp;
    Timestamp::Timebase base = {};
    bool ret =  serverStore->capturer_->GetTimestamp(timestamp, base);
    if (!ret) {
        MEDIA_ERR_LOG("No readtime get.");
        return false;
    }
    errno_t retCopy = memcpy_s((uint8_t *)buf, sizeof(Timestamp), &timestamp, sizeof(Timestamp));
    if (retCopy != EOK) {
        MEDIA_ERR_LOG("retCopy = %x", retCopy);
        return false;
    }
    surfaceBuf->SetSize(sizeof(Timestamp) + readLen);
    return true;
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
            usleep(WAIT_SURFACE_BUFFER_US);
            continue;
        }
        uint32_t size = serverStore->surface_->GetSize();
        void *buf = surfaceBuf->GetVirAddr();
        if (buf == nullptr) {
            serverStore->CancelBuffer(surfaceBuf);
            continue;
        }
        bool ret = serverStore->ReadAudioBuffer(serverStore, surfaceBuf, buf, size);
        if (!ret) {
            continue;
        }

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

std::string AudioCapturerServer::SerializeCaptureInfo(const AudioCapturerInfo &info)
{
    std::stringstream ss;
    ss << static_cast<int32_t>(info.inputSource) << ' '
        << static_cast<int32_t>(info.audioFormat) << ' '
        << info.sampleRate << ' '
        << info.channelCount << ' '
        << info.bitRate << ' '
        << info.deviceId << ' '
        << static_cast<int32_t>(info.streamType) << ' '
        << static_cast<int32_t>(info.bitWidth) << ' '
        << static_cast<int32_t>(info.deviceType);
    return ss.str();
}

AudioCapturerInfo AudioCapturerServer::DeserializeCaptureInfo(const char *str)
{
    AudioCapturerInfo info = {};
    std::stringstream ss(str);
    int32_t inputSource = 0;
    int32_t audioFormat = 0;
    int32_t streamType = 0;
    int32_t bitWidth = 0;
    int32_t deviceType = 0;
    ss >> inputSource >> audioFormat >> info.sampleRate
        >> info.channelCount >> info.bitRate >> info.deviceId
        >> streamType >> bitWidth >> deviceType;
    info.inputSource = static_cast<AudioSourceType>(inputSource);
    info.audioFormat = static_cast<AudioCodecFormat>(audioFormat);
    info.streamType = static_cast<AudioStreamType>(streamType);
    info.bitWidth = static_cast<AudioBitWidth>(bitWidth);
    info.deviceType = static_cast<AudioSystemDeviceType>(deviceType);
    return info;
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
    info = DeserializeCaptureInfo(static_cast<const char *>(bufferAdd));
    MEDIA_INFO_LOG("info.deviceId = %s, size = %d\n", info.deviceId.c_str(), size);
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
    std::string value = SerializeCaptureInfo(info);
    WriteUint32(reply, value.size());
    WriteBuffer(reply, value.c_str(), value.size());
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

void AudioCapturerServer::DispatchException(int32_t funcId, AudioCapturerImpl *capturer, IpcIo *req, IpcIo *reply)
{
    if (capturer == nullptr) {
        MEDIA_ERR_LOG("DispatchException failed, capturer value is nullptr");
        return;
    }
    switch (funcId) {
        case AUD_CAP_FUNC_START:
            Start(capturer, reply);
            break;
        case AUD_CAP_FUNC_STOP:
            Stop(capturer, reply);
            break;
        case AUD_CAP_FUNC_RELEASE:
            WriteInt32(reply, static_cast<int32_t>(capturer->Release()));
            break;
        case AUD_CAP_FUNC_SET_SURFACE:
            SetSurface(req, reply);
            break;
        case AUD_CAP_FUNC_GET_MIN_FRAME_COUNT:
            GetMiniFrameCount(req, reply);
            break;
        case AUD_CAP_FUNC_SET_DEVICE_CHANGE_CALLBACK:
            SetAudioCapturerServerCallback(capturer, req);
            break;
        default:
            break;
    }
}

void AudioCapturerServer::Dispatch(int32_t funcId, pid_t pid, IpcIo *req, IpcIo *reply)
{
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
        default:
            DispatchException(funcId, capturer, req, reply);
            break;
    }
}
}  // namespace Audio
}  // namespace OHOS
