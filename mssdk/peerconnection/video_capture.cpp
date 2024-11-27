#include "video_capture.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "lazyutil/lbscale.h"
//#include <libyuv.h>

BaseCapture::BaseCapture(int width, int height, int fps, int index) {
    Init(width, height, fps, index);
}
BaseCapture::~BaseCapture() {}
int BaseCapture::Init(int width, int height, int fps, int index) {
    width_ = width;
    height_ = height;
    fps_ = fps;
    index_ = index;
    return 0;
}

int BaseCapture::Deinit() {
    return 0;
}

void BaseCapture::AddOrUpdateSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
    webrtc::MutexLock lock(&lock_);
    broadcaster_.AddOrUpdateSink(sink, wants);
    UpdateVideoAdapter();
}
void BaseCapture::RemoveSink(
    rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) {
    webrtc::MutexLock lock(&lock_);
    broadcaster_.RemoveSink(sink);
    UpdateVideoAdapter();
}

void BaseCapture::OnFrame(const webrtc::VideoFrame& org_frame) {}
rtc::VideoSinkWants BaseCapture::GetSinkWants() {
    return broadcaster_.wants();
}

void BaseCapture::UpdateVideoAdapter() {}

CameraCapture::CameraCapture(int width, int height, int fps, int index)
    : BaseCapture(width, height, fps, index) {
    Init(width, height, fps, index);
}

CameraCapture::~CameraCapture() {
    Deinit();
}

int CameraCapture::Init(int width, int height, int fps, int index)

{
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> device_info(
        webrtc::VideoCaptureFactory::CreateDeviceInfo());

    char device_name[256];
    char unique_name[256];
    if (device_info->GetDeviceName(static_cast<uint32_t>(index), device_name,
        sizeof(device_name), unique_name,
        sizeof(unique_name)) != 0) {
        Deinit();
        RTC_LOG(INFO) << "GetDeviceName failed by index:" << index;
        return false;
    }

    vccap_ = webrtc::VideoCaptureFactory::Create(unique_name);
    if (!vccap_) {
        RTC_LOG(INFO) << "create video camera failed";
        return false;
    }
    vccap_->RegisterCaptureDataCallback(this);
    uint32_t num = device_info->NumberOfCapabilities(vccap_->CurrentDeviceName());
    for (uint32_t i = 0; i < num; i++) {
        device_info->GetCapability(vccap_->CurrentDeviceName(), i, capability_);
        RTC_LOG(LS_INFO) << "width:" << capability_.width
            << ", height:" << capability_.height
            << ", fps:" << capability_.maxFPS
            << ", videoType:" << capability_.videoType;
    }
    device_info->GetCapability(vccap_->CurrentDeviceName(), 0, capability_);

    capability_.width = static_cast<int32_t>(width);
    capability_.height = static_cast<int32_t>(height);
    capability_.maxFPS = static_cast<int32_t>(fps);
    capability_.videoType = webrtc::VideoType::kI420;

    if (vccap_->StartCapture(capability_) != 0) {
        // Destroy();
        RTC_LOG(LS_ERROR) << "start capture failed";
        return false;
    }

    RTC_CHECK(vccap_->CaptureStarted());

    return true;
}

int CameraCapture::Deinit() {
    webrtc::MutexLock lock(&lock_);
    vccap_ = nullptr;
    return 0;
}

int CameraCapture::Start()
{
    if (vccap_)
    {
        if (vccap_->StartCapture(capability_) != 0) {
            // Destroy();
            RTC_LOG(LS_ERROR) << "start capture failed";
            return -1;
        }

        RTC_CHECK(vccap_->CaptureStarted());
        return 0;
    }
    return -1;
}

void CameraCapture::Stop()
{
    if (vccap_)
    {
        vccap_->StopCapture();
    }
}

void CameraCapture::OnFrame(const webrtc::VideoFrame& org_frame) {
    int cropped_width = 0;
    int cropped_height = 0;
    int out_width = 0;
    int out_height = 0;

    webrtc::VideoFrame frame = org_frame;

    if (!video_adapter_.AdaptFrameResolution(
        frame.width(), frame.height(), frame.timestamp_us() * 1000,
        &cropped_width, &cropped_height, &out_width, &out_height)) {
        // Drop frame in order to respect frame rate constraint.
        return;
    }

    if (out_height != frame.height() || out_width != frame.width()) {
        // Video adapter has requested a down-scale. Allocate a new buffer and
        // return scaled version.
        // For simplicity, only scale here without cropping.
        rtc::scoped_refptr<webrtc::I420Buffer> scaled_buffer =
            webrtc::I420Buffer::Create(out_width, out_height);
        scaled_buffer->ScaleFrom(*frame.video_frame_buffer()->ToI420());
        webrtc::VideoFrame::Builder new_frame_builder =
            webrtc::VideoFrame::Builder()
            .set_video_frame_buffer(scaled_buffer)
            .set_rotation(webrtc::kVideoRotation_0)
            .set_timestamp_us(frame.timestamp_us())
            .set_id(frame.id());
        if (frame.has_update_rect()) {
            webrtc::VideoFrame::UpdateRect new_rect =
                frame.update_rect().ScaleWithFrame(frame.width(), frame.height(), 0,
                    0, frame.width(), frame.height(),
                    out_width, out_height);
            new_frame_builder.set_update_rect(new_rect);
        }
        broadcaster_.OnFrame(new_frame_builder.build());

    }
    else {
        // No adaptations needed, just return the frame as is.
        broadcaster_.OnFrame(frame);
    }
}

DestopCapture::DestopCapture(HWND hwnd,
    int width,
    int height,
    int fps,
    int index)
    : BaseCapture(width, height, fps, index), hwnd_(hwnd) {
    image_data_size_ = 0;
    image_width_ = 0;
    image_height_ = 0;
    image_data_ = NULL;
    Init(width, height, fps, index);
}

DestopCapture::~DestopCapture() {
    Deinit();
}

int DestopCapture::Init(int width, int height, int fps, int index) {
    webrtc::MutexLock lock(&lock_);
    if (NULL == hwnd_) {
        hwnd_ = GetDesktopWindow();
        RTC_LOG(LS_INFO) << "";
    }
    width_ = width;
    height_ = height;
    fps_ = fps;
    index_ = index;
    RECT rect;
    GetWindowRect(hwnd_, &rect);
    rtc::VideoSinkWants::FrameSize res(rect.right - rect.left,
        rect.bottom - rect.top);
    GetSinkWants().resolutions.push_back(res);
    int stride_y = width;
    int stride_uv = (width + 1) / 2;
    int target_width = width;
    int target_height = abs(height);
    buffer_ = webrtc::I420Buffer::Create(target_width, target_height, stride_y,
        stride_uv, stride_uv);
    scale_ = new lazyscale();
    //cap_thread_.reset(new std::thread(DestopCapture::WindowsCaptureProc, this));
    // cap_thread_->start();
    // cap_thread_->join();
    return 0;
}

int DestopCapture::Deinit() {
    // webrtc::MutexLock lock(&lock_);
    bRunning_ = false;
    Stop();
    //cap_thread_->join();
    if (scale_) {
        delete scale_;
        scale_ = NULL;
    }
    buffer_ = NULL;
    return 0;
}
void DestopCapture::OnFrame(const webrtc::VideoFrame& org_frame) {
    int cropped_width = 0;
    int cropped_height = 0;
    int out_width = 0;
    int out_height = 0;

    webrtc::VideoFrame frame = org_frame;

    if (!video_adapter_.AdaptFrameResolution(
        frame.width(), frame.height(), frame.timestamp_us() * 1000,
        &cropped_width, &cropped_height, &out_width, &out_height)) {
        // Drop frame in order to respect frame rate constraint.
        return;
    }
    broadcaster_.OnFrame(frame);
    /* for (size_t i = 0; i < sink_list_.size(); i++) {
      sink_list_[i]->OnFrame(org_frame);
    }VideoFrame captureFrame =
        VideoFrame::Builder()
            .set_video_frame_buffer(buffer)
            .set_timestamp_rtp(0)
            .set_timestamp_ms(rtc::TimeMillis())
            .set_rotation(!apply_rotation ? _rotateFrame : kVideoRotation_0)
            .build();
            rtc::scoped_refptr<I420Buffer> buffer = I420Buffer::Create(
        target_width, target_height, stride_y, stride_uv, stride_uv);
         int stride_y = width;
    int stride_uv = (width + 1) / 2;
    int target_width = width;
    int target_height = abs(height);*/
}
void DestopCapture::WindowsCaptureProc(void* owner) {
    DestopCapture* pthis = (DestopCapture*)owner;
    pthis->WindowCaptureProgress();
}

void DestopCapture::WindowCaptureProgress() {
    bRunning_ = true;
    RTC_LOG(LS_INFO) << "WindowCaptureProgress begin";
    while (bRunning_) {
        rtc::Thread::SleepMs(50);
        UpdateImage();
        DeliverImage(image_data_);
    }
    RTC_LOG(LS_INFO) << "WindowCaptureProgress end";
}

void DestopCapture::UpdateImage() {
    HDC hdc = GetWindowDC(hwnd_);
    int32_t width = 0;
    int32_t height = 0;
    BITMAP temp_bm;
    HBITMAP temp_hbmp = (HBITMAP)GetCurrentObject(hdc, OBJ_BITMAP);
    if (0 != GetObject(temp_hbmp, sizeof(BITMAP), &temp_bm)) {
        width = temp_bm.bmWidth;
        height = temp_bm.bmHeight;
    }
    else {
        RECT rect = { 0 };
        GetWindowRect(hwnd_, &rect);
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
    }
    DeleteObject(temp_hbmp);

    if (0 == width || 0 == height) {
        return;
    }
    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP win_bitmap = CreateCompatibleBitmap(hdc, width, height);
    SelectObject(mem_dc, win_bitmap);

    SetStretchBltMode(hdc, STRETCH_HALFTONE);
    BOOL capture_result = FALSE;
    /* if (base::win::GetVersion() > base::win::VERSION_WIN7 && !caputure_owner) {
      capture_result = PrintWindow(hwnd_, mem_dc, PRF_NONCLIENT);
    }*/
    if (!capture_result) {
        capture_result = StretchBlt(mem_dc, 0, 0, width, height, hdc, 0, 0, width,
            height, SRCCOPY | CAPTUREBLT);
    }

    if (capture_result) {
        // capture_fail_count_ = 0;

        /* RECT dwm_rect = {0};
        DwmGetWindowAttribute(hwnd_, DWMWA_EXTENDED_FRAME_BOUNDS, &dwm_rect,
                              sizeof(dwm_rect));
        int32_t dwm_width = dwm_rect.right - dwm_rect.left;
        int32_t dwm_height = dwm_rect.bottom - dwm_rect.top;*/
        RECT original_rect = { 0 };
        GetWindowRect(hwnd_, &original_rect);
        // int32_t original_width = original_rect.right - original_rect.left;
        // int32_t original_height = original_rect.bottom - original_rect.top;
        // double scale_x = (double)width / (double)original_width;
        // double scale_y = (double)height / (double)original_height;
        bool need_crop = false;
        HDC crop_dc = NULL;
        HBITMAP crop_bitmap = NULL;
        /* if ((dwm_width > 0 && dwm_height > 0) &&
            (width != dwm_width || height != dwm_height)) {
          need_crop = true;
        }

        if (need_crop) {
          int32_t off_x = 0;
          int32_t off_y = 0;
          off_x = scale_x * std::max(0L, dwm_rect.left - original_rect.left);
          off_y = scale_y * std::max(0L, dwm_rect.top - original_rect.top);
          width = std::min(width, (int32_t)(dwm_width * scale_x) +
                                      GetSystemMetrics(SM_CXBORDER));
          height = std::min(height, (int32_t)(dwm_height * scale_y) +
                                        GetSystemMetrics(SM_CYBORDER));

          crop_dc = CreateCompatibleDC(hdc);
          crop_bitmap = CreateCompatibleBitmap(hdc, width, height);
          SelectObject(crop_dc, crop_bitmap);
          SetStretchBltMode(mem_dc, STRETCH_HALFTONE);
          // int ret_bits =
          StretchBlt(crop_dc, 0, 0, width, height, mem_dc, off_x,
                                    off_y, width, height, SRCCOPY | CAPTUREBLT);
        }*/

        int bit_count = GetDeviceCaps(hdc, BITSPIXEL);
        BITMAPINFO bmi;  //    = {0};
        memset(&bmi, 0, sizeof(bmi));
        InitBitMapInfo(width, height, true, bit_count, &bmi);
        if (bmi.bmiHeader.biSizeImage != image_data_size_ &&
            bmi.bmiHeader.biWidth != 0 && bmi.bmiHeader.biHeight != 0) {
            if (image_data_) {
                delete[] image_data_;
                image_data_ = NULL;
            }
            image_data_size_ = bmi.bmiHeader.biSizeImage;
            image_width_ = width;
            image_height_ = height;
            image_data_ = new (std::nothrow) uint8_t[image_data_size_];
            if (image_data_ == nullptr) {
                return;
            }
            memset(image_data_, 0, image_data_size_);
        }

        GetDIBits(need_crop ? crop_dc : mem_dc,
            need_crop ? crop_bitmap : win_bitmap, 0, height, image_data_,
            &bmi, DIB_RGB_COLORS);
        /* if (scale_) {
          scale_->scale(image_data_, libyuv::FOURCC_BGRA, width, height,
                        (uint8_t*)buffer_->DataY(),
                        buffer_->width() * buffer_->height() * 3 / 2,
                        libyuv::FOURCC_I420, buffer_->width(), buffer_->height());
          frame.set_video_frame_buffer(buffer_);
        }*/
        DeleteObject(win_bitmap);
        if (crop_dc) {
            DeleteDC(crop_dc);
        }
        if (crop_bitmap) {
            DeleteObject(crop_bitmap);
        }
        if (mem_dc) {
            DeleteDC(mem_dc);
        }
        if (hdc) {
            ReleaseDC(hwnd_, hdc);
        }
    }
}

int DestopCapture::DeliverImage(uint8_t* pimg) {
    webrtc::VideoFrame frame = webrtc::VideoFrame::Builder()
        .set_video_frame_buffer(buffer_)
        .set_timestamp_rtp(0)
        .set_timestamp_ms(rtc::TimeMillis())
        .set_rotation(webrtc::kVideoRotation_0)
        .build();
    if (scale_) {
        scale_->scale(image_data_, libyuv::FOURCC_BGRA, image_width_, image_height_,
            (uint8_t*)buffer_->DataY(),
            buffer_->width() * buffer_->height() * 3 / 2,
            libyuv::FOURCC_I420, buffer_->width(), buffer_->height());
        frame.set_video_frame_buffer(buffer_);
    }
    OnFrame(frame);
    return 0;
}

int DestopCapture::CaptureWindowsImage() {
    webrtc::VideoFrame frame = webrtc::VideoFrame::Builder()
        .set_video_frame_buffer(buffer_)
        .set_timestamp_rtp(0)
        .set_timestamp_ms(rtc::TimeMillis())
        .set_rotation(webrtc::kVideoRotation_0)
        .build();
    if (image_data_) {
        frame.set_video_frame_buffer(buffer_);
        OnFrame(frame);
        return 0;
    }
    HDC hdc = GetWindowDC(hwnd_);
    int32_t width = 0;
    int32_t height = 0;
    BITMAP temp_bm;
    HBITMAP temp_hbmp = (HBITMAP)GetCurrentObject(hdc, OBJ_BITMAP);
    if (0 != GetObject(temp_hbmp, sizeof(BITMAP), &temp_bm)) {
        width = temp_bm.bmWidth;
        height = temp_bm.bmHeight;
    }
    else {
        RECT rect = { 0 };
        GetWindowRect(hwnd_, &rect);
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
    }
    DeleteObject(temp_hbmp);

    if (0 == width || 0 == height) {
        return -1;
    }
    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP win_bitmap = CreateCompatibleBitmap(hdc, width, height);
    SelectObject(mem_dc, win_bitmap);

    SetStretchBltMode(hdc, STRETCH_HALFTONE);
    BOOL capture_result = FALSE;
    /* if (base::win::GetVersion() > base::win::VERSION_WIN7 && !caputure_owner) {
      capture_result = PrintWindow(hwnd_, mem_dc, PRF_NONCLIENT);
    }*/
    if (!capture_result) {
        capture_result = StretchBlt(mem_dc, 0, 0, width, height, hdc, 0, 0, width,
            height, SRCCOPY | CAPTUREBLT);
    }

    if (capture_result) {
        // capture_fail_count_ = 0;

        /* RECT dwm_rect = {0};
        DwmGetWindowAttribute(hwnd_, DWMWA_EXTENDED_FRAME_BOUNDS, &dwm_rect,
                              sizeof(dwm_rect));
        int32_t dwm_width = dwm_rect.right - dwm_rect.left;
        int32_t dwm_height = dwm_rect.bottom - dwm_rect.top;*/
        RECT original_rect = { 0 };
        GetWindowRect(hwnd_, &original_rect);
        // int32_t original_width = original_rect.right - original_rect.left;
        // int32_t original_height = original_rect.bottom - original_rect.top;
        // double scale_x = (double)width / (double)original_width;
        // double scale_y = (double)height / (double)original_height;
        bool need_crop = false;
        HDC crop_dc = NULL;
        HBITMAP crop_bitmap = NULL;
        /* if ((dwm_width > 0 && dwm_height > 0) &&
            (width != dwm_width || height != dwm_height)) {
          need_crop = true;
        }

        if (need_crop) {
          int32_t off_x = 0;
          int32_t off_y = 0;
          off_x = scale_x * std::max(0L, dwm_rect.left - original_rect.left);
          off_y = scale_y * std::max(0L, dwm_rect.top - original_rect.top);
          width = std::min(width, (int32_t)(dwm_width * scale_x) +
                                      GetSystemMetrics(SM_CXBORDER));
          height = std::min(height, (int32_t)(dwm_height * scale_y) +
                                        GetSystemMetrics(SM_CYBORDER));

          crop_dc = CreateCompatibleDC(hdc);
          crop_bitmap = CreateCompatibleBitmap(hdc, width, height);
          SelectObject(crop_dc, crop_bitmap);
          SetStretchBltMode(mem_dc, STRETCH_HALFTONE);
          // int ret_bits =
          StretchBlt(crop_dc, 0, 0, width, height, mem_dc, off_x,
                                    off_y, width, height, SRCCOPY | CAPTUREBLT);
        }*/

        int bit_count = GetDeviceCaps(hdc, BITSPIXEL);
        BITMAPINFO bmi;  //    = {0};
        memset(&bmi, 0, sizeof(bmi));
        InitBitMapInfo(width, height, true, bit_count, &bmi);
        if (bmi.bmiHeader.biSizeImage != image_data_size_ &&
            bmi.bmiHeader.biWidth != 0 && bmi.bmiHeader.biHeight != 0) {
            if (image_data_) {
                delete[] image_data_;
                image_data_ = NULL;
            }
            image_data_size_ = bmi.bmiHeader.biSizeImage;
            image_width_ = width;
            image_height_ = height;
            image_data_ = new (std::nothrow) uint8_t[image_data_size_];
            if (image_data_ == nullptr) {
                return -1;
            }
            memset(image_data_, 0, image_data_size_);
            /*if(NULL  == frame_)
            {
              frame_ = new lbframe();
              frame_->init_video(libyuv::FOURCC_BGRA, width, height, 0);
            }*/
        }

        /* if (enable_capture_cursor_) {
          RECT win_rect = {0, 0, width, height};
          CaptureCursor(capture_wnd, need_crop ? crop_dc : mem_dc, win_rect,
                        scale_x, scale_y);
        }*/

        GetDIBits(need_crop ? crop_dc : mem_dc,
            need_crop ? crop_bitmap : win_bitmap, 0, height, image_data_,
            &bmi, DIB_RGB_COLORS);
        if (scale_) {
            /* static bool firstframe = true;
            if (firstframe) {
              firstframe = false;
              lazybitmap bmp;
              bmp.write_bitmap("first.bmp", image_data_, width * height * 4, width,
                               height, 32);
            }*/
            // std::unique_ptr<DibImage> dib_image(new DibImage(&bmi, image_data_));
            // dib_image->Save(L"D:\\testtt.bmp");
            // scale(uint8_t * psrc, uint32_t srcfoucc, uint32_t srcw, uint32_t
            // srch,uint8_t * pdst, int len, uint32_t dstfoucc, uint32_t dstw =
            // 0,uint32_t dsth = 0) uint32_t src_format =
            // (uint32_t)libyuv::FOURCC_BGRA; uint32_t dst_format =
            // (uint32_t)libyuv::FOURCC_I420;
            scale_->scale(image_data_, libyuv::FOURCC_BGRA, width, height,
                (uint8_t*)buffer_->DataY(),
                buffer_->width() * buffer_->height() * 3 / 2,
                libyuv::FOURCC_I420, buffer_->width(), buffer_->height());
            frame.set_video_frame_buffer(buffer_);
        }
        DeleteObject(win_bitmap);
        if (crop_dc) {
            DeleteDC(crop_dc);
        }
        if (crop_bitmap) {
            DeleteObject(crop_bitmap);
        }
        if (mem_dc) {
            DeleteDC(mem_dc);
            mem_dc = NULL;
        }

        if (hdc) {
            ReleaseDC(hwnd_, hdc);
            hdc = NULL;
        }
    }

    OnFrame(frame);
    return 0;
}

int DestopCapture::Start()
{
    if (!cap_thread_)
    {
        cap_thread_.reset(new std::thread(DestopCapture::WindowsCaptureProc, this));
        return 0;
    }
    return 1;
}

void DestopCapture::Stop()
{
    cap_thread_->join();
    cap_thread_.reset();
}

void DestopCapture::InitBitMapInfo(int width,
    int height,
    bool top2bottom,
    int bit_count,
    BITMAPINFO* bmi) {
    BITMAPINFOHEADER* bih = &bmi->bmiHeader;
    // InitBitMapInfoHeader(width, height, top2bottom, bit_count,
    // &bmi->bmiHeader);
    memset(bih, 0, sizeof(BITMAPINFOHEADER));
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biWidth = width;
    bih->biHeight = top2bottom ? -height : height;
    bih->biPlanes = 1;
    bih->biBitCount = bit_count;
    bih->biCompression = BI_RGB;
    bih->biSizeImage = width * height * 4;
    memset(bmi->bmiColors, 0, sizeof(RGBQUAD));
}

rtc::scoped_refptr<CustomCaptureTrackSource> CustomCaptureTrackSource::create(
    HWND hwnd,
    int width,
    int height,
    int fps,
    int index) {
    std::unique_ptr<BaseCapture> capturer = absl::WrapUnique(
        // new H264Loader("..//out2.h264"));
#if 1
        (BaseCapture*)new DestopCapture(hwnd, width, height, fps, index));
#else
        (BaseCapture*)new CameraCapture(/* hwnd,*/ width, height, fps, index));
#endif
    if (capturer) {
        rtc::scoped_refptr<CustomCaptureTrackSource> ccts =
            new rtc::RefCountedObject<CustomCaptureTrackSource>(
                std::move(capturer));
        // rtc::scoped_refptr<CapturerTrackSource> cts =
        //    new rtc::RefCountedObject<CapturerTrackSource>(capturer);
        ccts->Start();
        return ccts;
    }
    return nullptr;
}

int CustomCaptureTrackSource::Start()
{
    return capturer_->Start();
}

void CustomCaptureTrackSource::Stop()
{
    capturer_->Stop();
}