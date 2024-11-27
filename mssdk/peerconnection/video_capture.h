#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "media/base/video_adapter.h"
#include "media/base/video_broadcaster.h"
#include "modules/video_capture/video_capture_factory.h"
#include "rtc_base/event.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/synchronization/mutex.h"
#include "api/scoped_refptr.h"
//#include "modules/video_capture/video_capture.h"
#include "pc/video_track_source.h"
//#include "modules/video_capture/video_capture_factory.h"

class BaseCapture : public rtc::VideoSourceInterface<webrtc::VideoFrame>,
    public rtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
    BaseCapture(int width, int height, int fps, int index);
    ~BaseCapture();
    virtual int Init(int width, int height, int fps, int index);
    virtual int Deinit();
    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
        const rtc::VideoSinkWants& wants) override;
    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;

    virtual int Start() = 0;

    virtual void Stop() = 0;
protected:
    virtual void OnFrame(const webrtc::VideoFrame& org_frame) override;
    rtc::VideoSinkWants GetSinkWants();

protected:
    void UpdateVideoAdapter();

    webrtc::Mutex lock_;
    rtc::VideoBroadcaster broadcaster_;
    cricket::VideoAdapter video_adapter_;

    int width_;
    int height_;
    int fps_;
    int index_;
};

class CameraCapture : public BaseCapture {
public:
    CameraCapture(int width, int height, int fps, int index);
    ~CameraCapture();
    virtual int Init(int width, int height, int fps, int index) override;
    virtual int Deinit() override;

    virtual int Start();

    virtual void Stop();

protected:
    void OnFrame(const webrtc::VideoFrame& org_frame) override;
    rtc::scoped_refptr<webrtc::VideoCaptureModule> vccap_;
    webrtc::VideoCaptureCapability capability_;
};

class DestopCapture : public BaseCapture {
public:
    DestopCapture(HWND hwnd, int width, int height, int fps, int index);
    ~DestopCapture();
    virtual int Init(int width, int height, int fps, int index) override;
    virtual int Deinit() override;

    void InitBitMapInfo(int width,
        int height,
        bool top2bottom,
        int bit_count,
        BITMAPINFO* bmi);
    int CaptureWindowsImage();

    virtual int Start();

    virtual void Stop();

protected:
    void OnFrame(const webrtc::VideoFrame& org_frame) override;
    static void WindowsCaptureProc(void* owner);

    void WindowCaptureProgress();
    void UpdateImage();
    int DeliverImage(uint8_t* pimg);

protected:
    HWND hwnd_;
    DWORD image_data_size_;
    int image_width_;
    int image_height_;
    uint8_t* image_data_;
    // rtc::scoped_refptr<webrtc::I420Buffer> buffer_;
    rtc::scoped_refptr<webrtc::I420Buffer> buffer_;
    class lazyscale* scale_;
    // class lbframe* frame_;
    std::unique_ptr<std::thread> cap_thread_;
    bool bRunning_;
};

class CustomCaptureTrackSource : public webrtc::VideoTrackSource {
public:
    static rtc::scoped_refptr<CustomCaptureTrackSource> create(HWND hwnd,
        int width,
        int height,
        int fps,
        int index);

    /* static rtc::scoped_refptr<WindowCaptureTrackSource>
   CreateCameraCapture(int width,
                                                              int height,
                                                              int fps,
                                                              int index);*/
    virtual int Start();

    virtual void Stop();
protected:
    explicit CustomCaptureTrackSource(std::unique_ptr<BaseCapture> capturer)
        : VideoTrackSource(/*remote=*/false), capturer_(std::move(capturer)) {}

private:
    rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override {
        return capturer_.get();
    }
    std::unique_ptr<BaseCapture> capturer_;
};