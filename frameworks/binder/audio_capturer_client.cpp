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

#include "audio_capturer_client.h"

#include <cstdio>
#include <sstream>
#include "audio_capturer_server.h"
#include "media_log.h"
#include "ipc_skeleton.h"
#include "samgr_lite.h"
#include "securec.h"
#include "surface_impl.h"

extern "C" void __attribute__((weak)) OHOS_SystemInit(void)
{
    SAMGR_Bootstrap();
}

using namespace OHOS::Media;
using namespace std;
namespace OHOS {
namespace Audio {
constexpr int32_t SURFACE_QUEUE_SIZE = 5;
// hihopeos add record buffer use surface buffer
// when channel = 2, ai output buffer 2048*2 + 12 > 4096, surface size must be 8192
constexpr int32_t SURFACE_SIZE = 8192;
constexpr int32_t SURFACE_HEIGHT = 1;
constexpr int32_t SURFACE_WIDTH = 8192;
constexpr int32_t WAIT_SURFACE_BUFFER_US = 10000;

struct CallBackPara {
    int funcId;
    int32_t ret;
    void* data;
};

AudioCapturer::AudioCapturerClient *AudioCapturer::AudioCapturerClient::GetInstance()
{
    static AudioCapturerClient client;
    return &client;
}

static bool IsValidAudioSourceType(int32_t value)
{
    switch (value) {
        case AUDIO_SOURCE_INVALID:
        case AUDIO_SOURCE_DEFAULT:
        case AUDIO_MIC:
        case AUDIO_VOICE_UPLINK:
        case AUDIO_VOICE_DOWNLINK:
        case AUDIO_VOICE_CALL:
        case AUDIO_CAMCORDER:
        case AUDIO_VOICE_RECOGNITION:
        case AUDIO_VOICE_COMMUNICATION:
        case AUDIO_REMOTE_SUBMIX:
        case AUDIO_UNPROCESSED:
        case AUDIO_VOICE_PERFORMANCE:
        case AUDIO_ECHO_REFERENCE:
        case AUDIO_RADIO_TUNER:
        case AUDIO_HOTWORD:
        case AUDIO_REMOTE_SUBMIX_EXTEND:
            return true;
        default:
            return false;
    }
}

static bool IsValidAudioCodecFormat(int32_t value)
{
    return value >= AUDIO_DEFAULT && value < FORMAT_BUTT;
}

static bool IsValidAudioStreamType(int32_t value)
{
    /* Upper bound is last AudioStreamType enumerator (TYPE_TTS + 1). */
    return value >= TYPE_DEFAULT && value <= TYPE_TTS + 1;
}

static bool IsValidAudioBitWidth(int32_t value)
{
    return value == BIT_WIDTH_8 || value == BIT_WIDTH_16 ||
        value == BIT_WIDTH_24 || value == BIT_WIDTH_32;
}

static bool IsValidAudioSystemDeviceType(int32_t value)
{
    return value >= AUDIO_DEVICE_MIC_LOCAL && value <= AUDIO_DEVICE_SPEAKER_VIRTUAL;
}

static bool DeserializeCaptureInfo(const char *str, AudioCapturerInfo *info)
{
    if (str == nullptr || info == nullptr) {
        MEDIA_ERR_LOG("DeserializeCaptureInfo invalid args");
        return false;
    }
    AudioCapturerInfo tmp = {};
    std::stringstream ss(str);
    int32_t inputSource = 0;
    int32_t audioFormat = 0;
    int32_t streamType = 0;
    int32_t bitWidth = 0;
    int32_t deviceType = 0;
    ss >> inputSource >> audioFormat >> tmp.sampleRate
        >> tmp.channelCount >> tmp.bitRate >> tmp.deviceId
        >> streamType >> bitWidth >> deviceType;
    if (ss.fail()) {
        MEDIA_ERR_LOG("DeserializeCaptureInfo parse failed");
        return false;
    }
    if (!IsValidAudioSourceType(inputSource) || !IsValidAudioCodecFormat(audioFormat) ||
        !IsValidAudioStreamType(streamType) || !IsValidAudioBitWidth(bitWidth) ||
        !IsValidAudioSystemDeviceType(deviceType)) {
        MEDIA_ERR_LOG("DeserializeCaptureInfo invalid enum: source=%d format=%d stream=%d "
            "bitWidth=%d deviceType=%d", inputSource, audioFormat, streamType, bitWidth, deviceType);
        return false;
    }
    tmp.inputSource = static_cast<AudioSourceType>(inputSource);
    tmp.audioFormat = static_cast<AudioCodecFormat>(audioFormat);
    tmp.streamType = static_cast<AudioStreamType>(streamType);
    tmp.bitWidth = static_cast<AudioBitWidth>(bitWidth);
    tmp.deviceType = static_cast<AudioSystemDeviceType>(deviceType);
    *info = tmp;
    return true;
}

static int32_t HandleProxyGetInfo(IpcIo *reply, CallBackPara *para)
{
    uint32_t size = 0;
    ReadUint32(reply, &size);
    void *bufferAdd = (void *)ReadBuffer(reply, (size_t)size);
    if (bufferAdd == nullptr || !size) {
        MEDIA_INFO_LOG("Readbuffer info failed");
        return -1;
    }
    if (!DeserializeCaptureInfo(static_cast<const char *>(bufferAdd),
        static_cast<AudioCapturerInfo *>(para->data))) {
        return -1;
    }
    return 0;
}

static int32_t HandleProxyCallbackResult(CallBackPara *para, IpcIo *reply)
{
    AudioCapturerFuncId funcId = (AudioCapturerFuncId)para->funcId;
    switch (funcId) {
        case AUD_CAP_FUNC_CONNECT:
        case AUD_CAP_FUNC_DISCONNECT:
        case AUD_CAP_FUNC_SET_INFO:
        case AUD_CAP_FUNC_START:
        case AUD_CAP_FUNC_STOP:
        case AUD_CAP_FUNC_RELEASE:
        case AUD_CAP_FUNC_SET_SURFACE:
        case AUD_CAP_FUNC_SET_DEVICE_CHANGE_CALLBACK:
            break;
        case AUD_CAP_FUNC_GET_FRAME_COUNT:
            ReadUint64(reply, reinterpret_cast<uint64_t *>(para->data));
            break;
        case AUD_CAP_FUNC_GET_STATUS:
        case AUD_CAP_FUNC_GET_MIN_FRAME_COUNT:
            ReadUint32(reply, reinterpret_cast<uint32_t *>(para->data));
            break;
        case AUD_CAP_FUNC_GET_INFO:
            return HandleProxyGetInfo(reply, para);
        default:
            MEDIA_INFO_LOG("Callback, unknown funcId = %d", para->funcId);
            break;
    }
    return 0;
}

static int32_t ProxyCallbackFunc(void *owner, int code, IpcIo *reply)
{
    if (code) {
        MEDIA_ERR_LOG("callback error, code = %d", code);
        return -1;
    }
    if (owner == nullptr) {
        return -1;
    }
    CallBackPara *para = static_cast<CallBackPara *>(owner);
    ReadInt32(reply, &para->ret);
    return HandleProxyCallbackResult(para, reply);
}

int32_t AudioCapturer::AudioCapturerClient::InitSurface(void)
{
    MEDIA_DEBUG_LOG("AudioCapturerClient InitSurface");
    Surface *surface = Surface::CreateSurface();
    if (surface == nullptr) {
        return -1;
    }

    surface->RegisterConsumerListener(*this);
    surface_.reset(surface);

    surface->SetWidthAndHeight(SURFACE_WIDTH, SURFACE_HEIGHT);
    surface->SetQueueSize(SURFACE_QUEUE_SIZE);
    surface->SetSize(SURFACE_SIZE);
    surface->SetUsage(BUFFER_CONSUMER_USAGE_HARDWARE);
    return 0;
}

int32_t AudioCapturer::AudioCapturerClient::DeleteSurface(void)
{
    /* release all surface buffer */
    if (surface_ == nullptr) {
        return -1;
    }
    ReleaseAllBuffer();
    surface_->UnregisterConsumerListener();
    surface_.reset();
    surface_ = nullptr;
    return 0;
}

AudioCapturer::AudioCapturerClient::AudioCapturerClient()
{
    ipcStubContext_ = std::make_shared<IpcStubContext>();
    OHOS_SystemInit();
    IpcIo io;
    uint8_t tmpData[DEFAULT_IPC_SIZE];
    IUnknown *iUnknown = SAMGR_GetInstance()->GetDefaultFeatureApi(AUDIO_CAPTURER_SERVICE_NAME);
    if (iUnknown == nullptr) {
        MEDIA_ERR_LOG("iUnknown is nullptr");
        throw runtime_error("Ipc proxy GetDefaultFeatureApi failed.");
    }

    (void)iUnknown->QueryInterface(iUnknown, CLIENT_PROXY_VER, (void **)&proxy_);
    if (proxy_ == nullptr) {
        MEDIA_ERR_LOG("QueryInterface failed");
        throw runtime_error("Ipc proxy init failed.");
    }

    IpcIoInit(&io, tmpData, DEFAULT_IPC_SIZE, 0);
    CallBackPara para = {.funcId = AUD_CAP_FUNC_CONNECT, .ret = MEDIA_IPC_FAILED, .data = this};
    int32_t ret = proxy_->Invoke(proxy_, AUD_CAP_FUNC_CONNECT, nullptr, &para, ProxyCallbackFunc);
    if (ret) {
        MEDIA_ERR_LOG("Connect audio capturer server failed, ret=%d", ret);
        throw runtime_error("Ipc proxy Invoke failed.");
    }

    /* Creating a Surface and Initializing Settings */
    MEDIA_DEBUG_LOG("InitSurface audio capturer.");
    InitSurface();
    /* The surface is transferred to the server for processing */
    timeStampValid_ = false;
    IpcIoInit(&io, tmpData, DEFAULT_IPC_SIZE, 1);
    dynamic_cast<SurfaceImpl *>(surface_.get())->WriteIoIpcIo(io);
    para = {.funcId = AUD_CAP_FUNC_SET_SURFACE, .ret = MEDIA_IPC_FAILED, .data = this};
    ret = proxy_->Invoke(proxy_, AUD_CAP_FUNC_SET_SURFACE, &io, &para, ProxyCallbackFunc);
    if (ret) {
        MEDIA_ERR_LOG("Connect audio capturer set surface failed, ret=%d", ret);
        throw runtime_error("Ipc proxy Invoke failed.");
    }

    MEDIA_INFO_LOG("Create audio capturer client succeed.");
}

void AudioCapturer::AudioCapturerClient::ReleaseAllBuffer()
{
    timeStampValid_ = false;
    while (true) {
        SurfaceBuffer *surfaceBuf = surface_->AcquireBuffer();
        if (surfaceBuf == nullptr) {
            break;
        }
        surface_->ReleaseBuffer(surfaceBuf);
    }
}

AudioCapturer::AudioCapturerClient::~AudioCapturerClient()
{
    IpcIo io;
    uint8_t tmpData[DEFAULT_IPC_SIZE];
    IpcIoInit(&io, tmpData, DEFAULT_IPC_SIZE, 0);
    CallBackPara para = {.funcId = AUD_CAP_FUNC_DISCONNECT, .ret = MEDIA_IPC_FAILED};
    uint32_t ret = proxy_->Invoke(proxy_, AUD_CAP_FUNC_DISCONNECT, &io, &para, ProxyCallbackFunc);
    if (ret) {
        MEDIA_ERR_LOG("Disconnect audioCapturer server failed, ret=%d", ret);
    }

    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        weakCallback_.reset();
        callback_.reset();
        objectStub_.args = nullptr;
    }
    if (ipcStubContext_ != nullptr) {
        std::lock_guard<std::mutex> lock(ipcStubContext_->mutex);
        ipcStubContext_->weakCallback.reset();
    }

    /* release all surface buffer */
    if (surface_ != nullptr) {
        DeleteSurface();
    }
    MEDIA_INFO_LOG("destructor");
}

bool AudioCapturer::AudioCapturerClient::GetMinFrameCount(int32_t sampleRate, int32_t channelCount,
                                                          AudioCodecFormat audioFormat, size_t &frameCount)
{
    AudioCapturerClient *client = AudioCapturer::AudioCapturerClient::GetInstance();
    IpcIo io;
    uint8_t tmpData[DEFAULT_IPC_SIZE];
    IpcIoInit(&io, tmpData, DEFAULT_IPC_SIZE, 0);
    WriteInt32(&io, sampleRate);
    WriteInt32(&io, channelCount);
    WriteUint32(&io, sizeof(audioFormat));
    WriteBuffer(&io, &audioFormat, sizeof(audioFormat));
    CallBackPara para = {.funcId = AUD_CAP_FUNC_GET_MIN_FRAME_COUNT, .ret = MEDIA_IPC_FAILED, .data = &frameCount};
    uint32_t ret = client->proxy_->Invoke(client->proxy_, AUD_CAP_FUNC_GET_MIN_FRAME_COUNT, &io, &para,
                                        ProxyCallbackFunc);
    if (ret) {
        MEDIA_ERR_LOG("GetMinFrameCount failed, ret=%d", ret);
        return false;
    }
    return (!para.ret) ? true : false;
}

uint64_t AudioCapturer::AudioCapturerClient::GetFrameCount()
{
    IpcIo io;
    uint64_t frameCount;
    uint8_t tmpData[DEFAULT_IPC_SIZE];
    IpcIoInit(&io, tmpData, DEFAULT_IPC_SIZE, 0);
    CallBackPara para = {.funcId = AUD_CAP_FUNC_GET_FRAME_COUNT, .ret = MEDIA_IPC_FAILED, .data = &frameCount};

    if (proxy_ == nullptr) {
        MEDIA_ERR_LOG("GetFrameCount failed, proxy_ value is nullptr");
        return 0;
    }

    uint32_t ret = proxy_->Invoke(proxy_, AUD_CAP_FUNC_GET_FRAME_COUNT, &io, &para, ProxyCallbackFunc);
    if (ret) {
        MEDIA_ERR_LOG("GetFrameCount failed, ret=%d", ret);
    }

    return frameCount;
}

State AudioCapturer::AudioCapturerClient::GetStatus()
{
    IpcIo io;
    uint32_t state;
    uint8_t tmpData[DEFAULT_IPC_SIZE];
    IpcIoInit(&io, tmpData, DEFAULT_IPC_SIZE, 0);
    CallBackPara para = {.funcId = AUD_CAP_FUNC_GET_STATUS, .ret = MEDIA_IPC_FAILED, .data = &state};

    if (proxy_ == nullptr) {
        MEDIA_ERR_LOG("GetStatus failed, proxy_ value is nullptr");
        return (State)state;
    }

    uint32_t ret = proxy_->Invoke(proxy_, AUD_CAP_FUNC_GET_STATUS, &io, &para, ProxyCallbackFunc);
    if (ret) {
        MEDIA_ERR_LOG("GetStatus failed, ret=%d", ret);
    }

    return (State)state;
}

bool AudioCapturer::AudioCapturerClient::GetAudioTime(Timestamp &timestamp, Timestamp::Timebase base)
{
    timestamp = curTimestamp_;
    return true;
}

std::string AudioCapturer::AudioCapturerClient::SerializeCaptureInfo(const AudioCapturerInfo &info)
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
    MEDIA_INFO_LOG("info.streamType = %d, info.bitWidth = %d, info.deviceType = %d",
        info.streamType, info.bitWidth, info.deviceType);
    return ss.str();
}

int32_t AudioCapturer::AudioCapturerClient::SetCapturerInfo(const AudioCapturerInfo &info)
{
    IpcIo io;
    uint8_t tmpData[DEFAULT_IPC_SIZE];
    IpcIoInit(&io, tmpData, DEFAULT_IPC_SIZE, 0);
    std::string value = SerializeCaptureInfo(info);
    WriteUint32(&io, value.size());
    WriteBuffer(&io, value.c_str(), value.size());
    CallBackPara para = {.funcId = AUD_CAP_FUNC_SET_INFO, .ret = MEDIA_IPC_FAILED};

    if (proxy_ == nullptr) {
        MEDIA_ERR_LOG("SetCapturerInfo failed, proxy_ value is nullptr");
        return 0;
    }

    int32_t ret = proxy_->Invoke(proxy_, AUD_CAP_FUNC_SET_INFO, &io, &para, ProxyCallbackFunc);
    if (ret) {
        MEDIA_ERR_LOG("SetCapturerInfo failed, ret=%d", ret);
        return ret;
    }
    return para.ret;
}

int32_t AudioCapturer::AudioCapturerClient::GetCapturerInfo(AudioCapturerInfo &info)
{
    IpcIo io;
    uint8_t tmpData[DEFAULT_IPC_SIZE];
    IpcIoInit(&io, tmpData, DEFAULT_IPC_SIZE, 0);
    CallBackPara para = {.funcId = AUD_CAP_FUNC_GET_INFO, .ret = MEDIA_IPC_FAILED, .data = &info};

    if (proxy_ == nullptr) {
        MEDIA_ERR_LOG("GetCapturerInfo failed, proxy_ value is nullptr");
        return 0;
    }

    int32_t ret = proxy_->Invoke(proxy_, AUD_CAP_FUNC_GET_INFO, &io, &para, ProxyCallbackFunc);
    if (ret) {
        MEDIA_ERR_LOG("GetCapturerInfo failed, ret=%d", ret);
        return ret;
    }
    return para.ret;
}

bool AudioCapturer::AudioCapturerClient::Start()
{
    IpcIo io;
    uint8_t tmpData[DEFAULT_IPC_SIZE];
    IpcIoInit(&io, tmpData, DEFAULT_IPC_SIZE, 0);
    CallBackPara para = {.funcId = AUD_CAP_FUNC_START, .ret = MEDIA_IPC_FAILED};

    if (proxy_ == nullptr) {
        MEDIA_ERR_LOG("Start failed, proxy_ value is nullptr");
        return false;
    }

    int32_t ret = proxy_->Invoke(proxy_, AUD_CAP_FUNC_START, &io, &para, ProxyCallbackFunc);
    if (ret) {
        MEDIA_ERR_LOG("Start failed, ret=%d", ret);
        return false;
    }

    return para.ret;
}

bool AudioCapturer::AudioCapturerClient::Stop()
{
    IpcIo io;
    uint8_t tmpData[DEFAULT_IPC_SIZE];
    IpcIoInit(&io, tmpData, DEFAULT_IPC_SIZE, 0);
    CallBackPara para = {.funcId = AUD_CAP_FUNC_STOP, .ret = MEDIA_IPC_FAILED};

    if (proxy_ == nullptr) {
        MEDIA_ERR_LOG("Stop failed, proxy_ value is nullptr");
        return false;
    }

    int32_t ret = proxy_->Invoke(proxy_, AUD_CAP_FUNC_STOP, &io, &para, ProxyCallbackFunc);
    if (ret) {
        MEDIA_ERR_LOG("Stop failed, ret=%d", ret);
        return false;
    }

    return para.ret;
}

bool AudioCapturer::AudioCapturerClient::Release()
{
    IpcIo io;
    uint8_t tmpData[DEFAULT_IPC_SIZE];
    IpcIoInit(&io, tmpData, DEFAULT_IPC_SIZE, 0);
    CallBackPara para = {.funcId = AUD_CAP_FUNC_RELEASE, .ret = MEDIA_IPC_FAILED};

    if (proxy_ == nullptr) {
        MEDIA_ERR_LOG("Release failed, proxy_ value is nullptr");
        return false;
    }

    int32_t ret = proxy_->Invoke(proxy_, AUD_CAP_FUNC_RELEASE, &io, &para, ProxyCallbackFunc);
    if (ret) {
        MEDIA_ERR_LOG("Release failed, ret=%d", ret);
        return false;
    }

    DeleteSurface();
    return para.ret;
}

int32_t AudioCapturer::AudioCapturerClient::Read(uint8_t *buffer, size_t userSize, bool isBlockingRead)
{
    if (buffer == nullptr || !userSize) {
        return ERR_INVALID_READ;
    }
    int32_t readLen = ERR_INVALID_READ;
    do {
        SurfaceBuffer *surfaceBuf = surface_->AcquireBuffer();
        if (surfaceBuf == nullptr) {
            if (isBlockingRead) {
                usleep(WAIT_SURFACE_BUFFER_US);
                continue;
            } else {
                break;
            }
        }

        uint8_t *buf = static_cast<uint8_t *> (surfaceBuf->GetVirAddr());
        int32_t dataSize = surfaceBuf->GetSize();
        if (dataSize - sizeof(Timestamp) > userSize) {
            surface_->ReleaseBuffer(surfaceBuf);
            MEDIA_ERR_LOG("input buffer size too small.");
            break;
        }

        (void)memcpy_s(buffer, userSize, buf + sizeof(Timestamp), dataSize - sizeof(Timestamp));
        (void)memcpy_s(&curTimestamp_, sizeof(Timestamp), buf, sizeof(Timestamp));
        timeStampValid_ = true;

        surface_->ReleaseBuffer(surfaceBuf);
        readLen = dataSize - sizeof(Timestamp);
        break;
    } while (isBlockingRead);

    return readLen;
}

void AudioCapturer::AudioCapturerClient::OnBufferAvailable()
{
    if (surface_ == nullptr) {
        MEDIA_ERR_LOG("OnBufferAvailable failed, surface_ is nullptr");
        return;
    }
}

IClientProxy *AudioCapturer::AudioCapturerClient::GetIClientProxy()
{
    return proxy_;
}

static int32_t HandleDeviceChanged(IpcIo *data,
    const std::shared_ptr<AudioManagerDeviceChangeCallback> &callback)
{
    if (data == nullptr || callback == nullptr) {
        MEDIA_ERR_LOG("HandleDeviceChanged invalid args");
        return -1;
    }
    uint32_t dhId = 0;
    if (!ReadUint32(data, &dhId)) {
        MEDIA_ERR_LOG("HandleDeviceChanged read dhId failed");
        return -1;
    }
    uint32_t size = 0;
    if (!ReadUint32(data, &size)) {
        MEDIA_ERR_LOG("HandleDeviceChanged read size failed");
        return -1;
    }
    if (size == 0 || size > MAX_DEVICE_NAME_LEN) {
        MEDIA_ERR_LOG("HandleDeviceChanged invalid size=%u", size);
        return -1;
    }
    void *bufferAdd = (void *)ReadBuffer(data, (size_t)size);
    if (bufferAdd == nullptr) {
        MEDIA_ERR_LOG("HandleDeviceChanged ReadBuffer failed");
        return -1;
    }
    int32_t deviceType = 0;
    if (!ReadInt32(data, &deviceType)) {
        MEDIA_ERR_LOG("HandleDeviceChanged read deviceType failed");
        return -1;
    }
    int32_t connectStatus = 0;
    if (!ReadInt32(data, &connectStatus)) {
        MEDIA_ERR_LOG("HandleDeviceChanged read connectStatus failed");
        return -1;
    }
    AudioDeviceInfo info = {};
    info.dhId = dhId;
    uint32_t copyLen = (size >= MAX_DEVICE_NAME_LEN) ? (MAX_DEVICE_NAME_LEN - 1) : size;
    if (memcpy_s(info.deviceName, MAX_DEVICE_NAME_LEN, bufferAdd, copyLen) != EOK) {
        MEDIA_ERR_LOG("Copy deviceName failed");
        return -1;
    }
    info.deviceName[copyLen] = '\0';
    info.deviceType = static_cast<AudioSystemDeviceType>(deviceType);
    info.connectStatus = static_cast<DeviceConnectStatus>(connectStatus);
    callback->OnDeviceChange(info);
    MEDIA_INFO_LOG("AudioCapturerClient AudioCapturerCallback, ON_DEVICE_CHANGED success\n");
    return 0;
}

int32_t AudioCapturer::AudioCapturerClient::AudioCapturerCallback(uint32_t code, IpcIo *data,
    IpcIo *reply, MessageOption option)
{
    auto *stubContext = static_cast<IpcStubContext *>(option.args);
    if (stubContext == nullptr) {
        MEDIA_ERR_LOG("call back error, stubContext is null");
        return -1;
    }
    std::shared_ptr<AudioManagerDeviceChangeCallback> playerCallback;
    {
        std::lock_guard<std::mutex> lock(stubContext->mutex);
        playerCallback = stubContext->weakCallback.lock();
    }
    if (playerCallback == nullptr) {
        MEDIA_ERR_LOG("call back error, playerCallback is null");
        return -1;
    }
    MEDIA_INFO_LOG("AudioCapturerCallback, funcId=%d\n", code);
    switch (code) {
        case ON_DEVICE_CHANGED:
            return HandleDeviceChanged(data, playerCallback);
        case ON_READ_DATA_FAILED:
            playerCallback->OnReadDataFailed();
            MEDIA_INFO_LOG("AudioCapturerClient AudioCapturerCallback, ON_READ_DATA_FAILED success\n");
            break;
        default:
            MEDIA_ERR_LOG("unsupported funId\n");
            break;
    }
    return 0;
}

void AudioCapturer::AudioCapturerClient::SetDeviceChangeCallback( \
    const std::shared_ptr<AudioManagerDeviceChangeCallback> &callback)
{
    if (sid_ == nullptr) {
        sid_ = std::make_unique<SvcIdentity>();
    }
    if (ipcStubContext_ == nullptr) {
        ipcStubContext_ = std::make_shared<IpcStubContext>();
    }
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        callback_ = callback;
        weakCallback_ = callback_;
        {
            std::lock_guard<std::mutex> stubLock(ipcStubContext_->mutex);
            ipcStubContext_->weakCallback = callback_;
        }
        objectStub_.func = AudioCapturerCallback;
        objectStub_.args = ipcStubContext_.get();
        objectStub_.isRemote = false;
    }
    sid_->handle = IPC_INVALID_HANDLE;
    sid_->token = SERVICE_TYPE_ANONYMOUS;
    sid_->cookie = reinterpret_cast<uintptr_t>(&objectStub_);
    IpcIo io;
    uint8_t tmpData[DEFAULT_IPC_SIZE];
    IpcIoInit(&io, tmpData, DEFAULT_IPC_SIZE, 1);
    bool writeRemote = WriteRemoteObject(&io, sid_.get());
    if (!writeRemote) {
        return;
    }
    if (proxy_ == nullptr) {
        MEDIA_ERR_LOG("SetDeviceChangeCallback failed, proxy_ value is nullptr");
        return;
    }
    CallBackPara para = {};
    para.funcId = AUD_CAP_FUNC_SET_DEVICE_CHANGE_CALLBACK;
    uint32_t ans = proxy_->Invoke(proxy_, AUD_CAP_FUNC_SET_DEVICE_CHANGE_CALLBACK, &io, &para, ProxyCallbackFunc);
    if (ans != 0) {
        MEDIA_ERR_LOG("SetDeviceChangeCallback : Invoke failed, ret=%u\n", ans);
    }
}

}  // namespace Audio
}  // namespace OHOS
