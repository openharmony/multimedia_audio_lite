/*
 * Copyright (C) 2020-2021 Huawei Device Co., Ltd.
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
#include "audiolite_test.h"
#include "media_errors.h"

using namespace std;
using namespace OHOS::Audio;
using namespace testing::ext;


namespace OHOS {
const int32_t BUFFER_SIZE = 1024;
const int32_t SAMPLE_RATE = 44100;
const int32_t BIT_RATE = 1024;
const int32_t CHANNEL_COUNT = 1;

void AudioliteTest::SetUpTestCase(void) {}

void AudioliteTest::TearDownTestCase(void) {}

void AudioliteTest::SetUp()
{
    audioCapInfo.channelCount = CHANNEL_COUNT;
    audioCapInfo.sampleRate = SAMPLE_RATE;
    audioCapInfo.bitRate = BIT_RATE;
    audioCapInfo.inputSource = AUDIO_SOURCE_DEFAULT;
    audioCapInfo.bitWidth = BIT_WIDTH_16;
}

void AudioliteTest::TearDown() {}

/*
 * Feature: Audiolite
 * Function: audioCapturer
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Audio Capture Start() Test
 */
HWTEST_F(AudioliteTest, audio_lite_audioCapturer_start_test_001, Level1)
{
    bool capatureStatus = false;
    int64_t frameCount = -1;
    int32_t retStatus = RET_FAILURE;
    int32_t getStatus;
    AudioCapturer *audioCapturer = new AudioCapturer();

    retStatus = audioCapturer->SetCapturerInfo(audioCapInfo);
    EXPECT_EQ(RET_SUCCESS, retStatus);
    capatureStatus = audioCapturer->Start();
    EXPECT_EQ(true, capatureStatus);
    getStatus = 0x2; /* it should be RECORDING */
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(getStatus, retStatus);
    frameCount = audioCapturer->GetFrameCount();
    EXPECT_EQ(true, (frameCount > 0));
    capatureStatus = audioCapturer->Release();
    delete audioCapturer;
}

/*
 * Feature: Audiolite
 * Function: audioCapturer
 * SubFunction: NA
 * FunctionPoints: .
 * EnvConditions: NA
 * CaseDescription: Audio Capture Start() Test
 */
HWTEST_F(AudioliteTest, audio_lite_audioCapturer_start_test_002, Level1)
{
    bool capatureStatus = false;
    int32_t retStatus = RET_FAILURE;
    int32_t getStatus;
    AudioCapturer *audioCapturer = new AudioCapturer();

    retStatus = audioCapturer->SetCapturerInfo(audioCapInfo);
    EXPECT_EQ(RET_SUCCESS, retStatus);
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(0x1, retStatus); /* it should be PREPARED */
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(0x1, retStatus); /* it should be PREPARED */
    capatureStatus = audioCapturer->Start();
    EXPECT_EQ(true, capatureStatus);
    getStatus = 0x2; /* it should be RECORDING */
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(getStatus, retStatus);
    capatureStatus = audioCapturer->Stop();
    EXPECT_EQ(true, capatureStatus);
    capatureStatus = audioCapturer->Release();
    EXPECT_EQ(true, capatureStatus);
    capatureStatus = audioCapturer->Start();
    EXPECT_EQ(false, capatureStatus);
    capatureStatus = audioCapturer->Release();
    delete audioCapturer;
}

/*
 * Feature: Audiolite
 * Function: audioCapturer
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Audio Capture Stop ()
 */
HWTEST_F(AudioliteTest, audio_lite_audioCapturer_stop_test_001, Level1)
{
    bool capatureStatus = true;
    int32_t retStatus = RET_FAILURE;
    AudioCapturer *audioCapturer = new AudioCapturer();

    retStatus = audioCapturer->SetCapturerInfo(audioCapInfo);
    EXPECT_EQ(RET_SUCCESS, retStatus);
    capatureStatus = audioCapturer->Stop();
    EXPECT_EQ(false, capatureStatus);
    delete audioCapturer;
}

/*
 * Feature: Audiolite
 * Function: audioCapturer
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Audio Capture Stop ()
 */
HWTEST_F(AudioliteTest, audio_lite_audioCapturer_stop_test_002, Level1)
{
    bool capatureStatus = false;
    int32_t retStatus = RET_FAILURE;
    int32_t getStatus;
    AudioCapturer *audioCapturer = new AudioCapturer();

    retStatus = audioCapturer->SetCapturerInfo(audioCapInfo);
    EXPECT_EQ(RET_SUCCESS, retStatus);
    capatureStatus = audioCapturer->Start();
    EXPECT_EQ(true, capatureStatus);
    getStatus = 0x2; /* it should be RECORDING */
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(getStatus, retStatus);
    capatureStatus = audioCapturer->Stop();
    EXPECT_EQ(true, capatureStatus);
    getStatus = 0x3; /* it should be STOPPED */
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(getStatus, retStatus);
    capatureStatus = audioCapturer->Release();
    delete audioCapturer;
}

/*
 * Feature: Audiolite
 * Function: audioCapturer
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Audio Capture Release ()
 */
HWTEST_F(AudioliteTest, audio_lite_audioCapturer_release_test_001, Level1)
{
    bool capatureStatus = true;
    int32_t retStatus = RET_FAILURE;
    AudioCapturer *audioCapturer = new AudioCapturer();

    retStatus = audioCapturer->SetCapturerInfo(audioCapInfo);
    EXPECT_EQ(RET_SUCCESS, retStatus);
    capatureStatus = audioCapturer->Release();
    EXPECT_EQ(true, capatureStatus);
    delete audioCapturer;
}

/*
 * Feature: Audiolite
 * Function: audioCapturer
 * SubFunction: NA
 * FunctionPoints: .
 * EnvConditions: NA
 * CaseDescription: Audio Capture Release ()
 */
HWTEST_F(AudioliteTest, audio_lite_audioCapturer_release_test_002, Level1)
{
    bool capatureStatus = false;
    int32_t retStatus = RET_FAILURE;
    int32_t getStatus;
    AudioCapturer *audioCapturer = new AudioCapturer();

    retStatus = audioCapturer->SetCapturerInfo(audioCapInfo);
    EXPECT_EQ(RET_SUCCESS, retStatus);
    getStatus = 0x1; /* it should be PREPARED */
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(getStatus, retStatus);
    capatureStatus = audioCapturer->Start();
    EXPECT_EQ(true, capatureStatus);
    getStatus = 0x2; /* it should be RECORDING */
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(getStatus, retStatus);
    sleep(5);
    capatureStatus = audioCapturer->Stop();
    EXPECT_EQ(true, capatureStatus);
    getStatus = 0x3; /* it should be STOPPED */
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(getStatus, retStatus);
    sleep(5);
    capatureStatus = audioCapturer->Release();
    EXPECT_EQ(true, capatureStatus);
    getStatus = 0x4; /* it should be RELEASED */
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(getStatus, retStatus);
    sleep(5);
    delete audioCapturer;
}

/*
 * Feature: Audiolite
 * Function: audioCapturer
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Audio Capture Status ()
 */
HWTEST_F(AudioliteTest, audio_lite_audioCapturer_status_test_001, Level1)
{
    bool capatureStatus = false;
    int32_t retStatus = RET_FAILURE;
    int32_t getStatus;
    AudioCapturer *audioCapturer = new AudioCapturer();

    retStatus = audioCapturer->SetCapturerInfo(audioCapInfo);
    EXPECT_EQ(RET_SUCCESS, retStatus);
    getStatus = 0x1; /* it should be PREPARED */
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(getStatus, retStatus);
    capatureStatus = audioCapturer->Start();
    EXPECT_EQ(true, capatureStatus);
    getStatus = 0x2; /* it should be RECORDING */
    sleep(1);
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(getStatus, retStatus);
    capatureStatus = audioCapturer->Stop();
    EXPECT_EQ(true, capatureStatus);
    getStatus = 0x3; /* it should be STOPPED */
    sleep(1);
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(getStatus, retStatus);
    capatureStatus = audioCapturer->Release();
    delete audioCapturer;
}

/*
 * Feature: Audiolite
 * Function: audioCapturer
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Audio Capture Status ()
 */
HWTEST_F(AudioliteTest, audio_lite_audioCapturer_status_test_002, Level1)
{
    int32_t retStatus = RET_FAILURE;
    int32_t getStatus;
    AudioCapturer *audioCapturer = new AudioCapturer();

    retStatus = audioCapturer->SetCapturerInfo(audioCapInfo);
    EXPECT_EQ(RET_SUCCESS, retStatus);
    getStatus = 0x2;
    retStatus = audioCapturer->GetStatus(); /* it should be PREPARED */
    EXPECT_NE(getStatus, retStatus);
    delete audioCapturer;
}

/*
 * Feature: Audiolite
 * Function: audioCapturer
 * SubFunction: NA
 * FunctionPoints: NA
 * EnvConditions: NA
 * CaseDescription: Audio Capture read() Test
 */
HWTEST_F(AudioliteTest, audio_lite_audioCapturer_read_test_001, Level1)
{
    bool capatureStatus = false;
    bool isBlocking = true;
    int64_t frameCount = -1;
    int32_t retStatus = RET_FAILURE;
    int32_t getStatus;
    int32_t size = 2 * BUFFER_SIZE;
    uint8_t buffer[size];
    uint8_t *bufPtr = NULL;
    size_t userSize, readSize = 0;
    AudioCapturer *audioCapturer = new AudioCapturer();

    bufPtr = buffer;
    retStatus = audioCapturer->SetCapturerInfo(audioCapInfo);
    EXPECT_EQ(RET_SUCCESS, retStatus);
    capatureStatus = audioCapturer->Start();
    EXPECT_EQ(true, capatureStatus);
    getStatus = 0x2; /* it should be RECORDING */
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(getStatus, retStatus);
    frameCount = audioCapturer->GetFrameCount();
    EXPECT_EQ(true, (frameCount > 0));
    userSize = frameCount * audioCapInfo.channelCount * audioCapInfo.sampleRate;
    readSize = audioCapturer->Read(bufPtr, userSize, isBlocking);
    EXPECT_EQ(true, (readSize > 0));
    capatureStatus = audioCapturer->Release();
    delete audioCapturer;
}

 /*
 * Feature: Audiolite
 * Function: audioCapturer
 * SubFunction: NA
 * FunctionPoints: .
 * EnvConditions: NA
 * CaseDescription: Audio Capture GetMinFrameCount() Test
 */
HWTEST_F(AudioliteTest, audio_lite_audioCapturer_GetMinFrameCount_test_001, Level1)
{
    bool capatureStatus = false;
    size_t frameCount = -1;
    int32_t retStatus = RET_FAILURE;
    int32_t getStatus;
    AudioCapturer *audioCapturer = new AudioCapturer();

    retStatus = audioCapturer->SetCapturerInfo(audioCapInfo);
    EXPECT_EQ(RET_SUCCESS, retStatus);
    capatureStatus = audioCapturer->Start();
    EXPECT_EQ(true, capatureStatus);
    getStatus = 0x2; /* it should be RECORDING */
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(getStatus, retStatus);
    sleep(1);
    frameCount = audioCapturer->GetFrameCount();
    EXPECT_EQ(true, (frameCount > 0));
    capatureStatus = audioCapturer->GetMinFrameCount(audioCapInfo.sampleRate,
                                    audioCapInfo.channelCount, audioCapInfo.audioFormat, frameCount);
    EXPECT_EQ(true, capatureStatus);
    capatureStatus = audioCapturer->Release();
    delete audioCapturer;
}

 /*
 * Feature: Audiolite
 * Function: audioCapturer
 * SubFunction: NA
 * FunctionPoints: .
 * EnvConditions: NA
 * CaseDescription: Audio Capture GetAudioTime() Test
 */
HWTEST_F(AudioliteTest, audio_lite_audioCapturer_GetAudioTime_test_001, Level1)
{
    bool capatureStatus = false;
    int32_t retStatus = RET_FAILURE;
    int32_t getStatus;
    Timestamp timeStamp;
    Timestamp::Timebase base = Timestamp::Timebase::MONOTONIC;
    AudioCapturer *audioCapturer = new AudioCapturer();

    retStatus = audioCapturer->SetCapturerInfo(audioCapInfo);
    EXPECT_EQ(RET_SUCCESS, retStatus);
    capatureStatus = audioCapturer->Start();
    EXPECT_EQ(true, capatureStatus);
    sleep(2);
    getStatus = 0x2; /* it should be RECORDING */
    retStatus = audioCapturer->GetStatus();
    EXPECT_EQ(getStatus, retStatus);
    capatureStatus = audioCapturer->GetAudioTime(timeStamp, base);
    EXPECT_EQ(true, capatureStatus);
    capatureStatus = audioCapturer->Release();
    delete audioCapturer;
}
} // namespace OHOS

