/*
 * UVCCamera
 * library and sample to access to UVC web camera on non-rooted Android device
 *
 * Copyright (c) 2014-2017 saki t_saki@serenegiant.com
 * Copyright (c) 2024 vshcryabets@gmail.com
 *
 * File name: UVCPreview.h
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * All files in the folder are under this Apache License, Version 2.0.
 * Files in the jni/libjpeg, jni/libusb, jin/libuvc folder may have a different license, see the respective files.
*/
#pragma once

#include <jni.h>
#include "libusb.h"
#include "libuvc/libuvc.h"
#include "utilbase.h"
#include <stdint.h>
#include <mutex>
#include <condition_variable>
#include <list>
#include <thread>

#define DEFAULT_PREVIEW_WIDTH 640
#define DEFAULT_PREVIEW_HEIGHT 480
#define DEFAULT_PREVIEW_FPS_MIN 1
#define DEFAULT_PREVIEW_FPS_MAX 30
#define DEFAULT_PREVIEW_MODE 0
#define DEFAULT_BANDWIDTH 1.0f
#define PREVIEW_PIXEL_BYTES 4    // RGBA/RGBX


typedef uvc_error_t (*convFunc_t)(uvc_frame_t *in, uvc_frame_t *out);

#define PIXEL_FORMAT_RAW 0        // same as PIXEL_FORMAT_YUV
#define PIXEL_FORMAT_YUV 1
#define PIXEL_FORMAT_RGB565 2
#define PIXEL_FORMAT_RGBX 3
#define PIXEL_FORMAT_YUV20SP 4
#define PIXEL_FORMAT_NV21 5        // YVU420SemiPlanar

struct UvcPreviewFrame {
    uvc_frame_t *mFrame;
    std::chrono::steady_clock::time_point mTimestamp;
};

class UvcPreviewFailed: public std::exception {
public:
    enum Type {
        OK = 0,
        NO_FORMAT,
        START_STREAM_FAILED,
    };
private:
    Type error;
    std::string description;
public:
    UvcPreviewFailed(Type error, std::string description);
    UvcPreviewFailed(UvcPreviewFailed&);
    virtual ~UvcPreviewFailed() noexcept {};
    virtual const char *what() noexcept;
};

class UvcPreviewListener {
public:
    // will be called on each frame from UVC
    virtual void handleFrame(uint16_t deviceId,
                             const UvcPreviewFrame &frame) = 0;

    // will be called once from worker thread of the UVCPreviewBase
    virtual void onPreviewPrepared(uint16_t deviceId,
                                   uint16_t frameWidth,
                                   uint16_t frameHeight) = 0;

    // will be called once before worker thread finishing
    virtual void onPreviewFinished(uint16_t deviceId) = 0;
    virtual void onPreviewFailed(uint16_t deviceId, UvcPreviewFailed error) = 0;

    virtual void onFrameDropped(uint16_t deviceId, std::chrono::steady_clock::time_point timestamp) = 0;
};


class UVCPreviewBase {
protected:
    uvc_device_handle_t *mDeviceHandle;
    volatile bool mIsRunning;
    int requestWidth, requestHeight, requestMode;
    int requestFps;
    float requestBandwidth;
    uint16_t frameWidth, frameHeight;
    int frameMode;
    size_t frameBytes;
    pthread_mutex_t preview_mutex;
    pthread_cond_t preview_sync;
    std::list<UvcPreviewFrame> mPreviewFrames;
    pthread_mutex_t pool_mutex;
    std::list<uvc_frame_t *> mFramePool;
    volatile uint16_t allocatedFramesCounter = 0;
    std::thread mPreviewThread;
    uint16_t mDeviceId;
    UvcPreviewListener *mPreviewListener;
    int16_t mFramePoolSize;
    int16_t mMaxFramesQueue;
private:
    void clear_pool();

    static void uvc_preview_frame_callback(uvc_frame_t *frame, void *vptr_args);

    void addPreviewFrame(uvc_frame_t *frame, std::chrono::steady_clock::time_point timestamp);

    void clearPreviewFramesQueue();

    void previewThreadFunc();

protected:
    uvc_frame_t *get_frame(size_t data_bytes);

    void recycle_frame(uvc_frame_t *frame);

    const UvcPreviewFrame waitPreviewFrame();

public:
    UVCPreviewBase(uvc_device_handle_t *devh,
                   uint16_t deviceId,
                   UvcPreviewListener *previewListener,
                   int16_t framePoolSize = 8,
                   int16_t maxFramesQueue = 4);

    virtual ~UVCPreviewBase();

    inline const bool isRunning() const;

    /**
     *
     * @param fps 0 - any FPS
     */
    int setPreviewSize(int width, int height, int fps, int mode, float bandwidth = 1.0f);

    int startPreview();

    virtual int stopPreview();
};
