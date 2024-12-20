/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EXAMPLES_PEERCONNECTION_CLIENT_MAIN_WND_H_
#define EXAMPLES_PEERCONNECTION_CLIENT_MAIN_WND_H_

#include <map>
#include <memory>
#include <string>

#include "api/media_stream_interface.h"
#include "api/video/video_frame.h"
//#include "examples/peerconnection/client/peer_connection_client.h"
#include "media/base/media_channel.h"
#include "media/base/video_common.h"
#if defined(WEBRTC_WIN)
#include "rtc_base/win32.h"
//#include "service/i_room_client.h"
//#include "service/i_participant_event_handler.h"
#include "api/video/i420_buffer.h"
#include "IRoomObserver.h"

#endif  // WEBRTC_WIN
typedef std::map<int, std::string> Peers;
class MainWndCallback {
 public:
  virtual void StartLogin(const std::string& server, int port) = 0;
  virtual void DisconnectFromServer() = 0;
  virtual void ConnectToPeer(int peer_id) = 0;
  virtual void DisconnectFromCurrentPeer() = 0;
  virtual void UIThreadCallback(int msg_id, void* data) = 0;
  virtual void Close() = 0;

 protected:
  virtual ~MainWndCallback() {}
};

// Pure virtual interface for the main window.
class MainWindow {
 public:
  virtual ~MainWindow() {}

  enum UI {
    CONNECT_TO_SERVER,
    LIST_PEERS,
    STREAMING,
  };

  virtual void RegisterObserver(MainWndCallback* callback) = 0;

  virtual bool IsWindow() = 0;
  virtual void MessageBox(const char* caption,
                          const char* text,
                          bool is_error) = 0;

  virtual UI current_ui() = 0;

  virtual void SwitchToConnectUI() = 0;
  virtual void SwitchToPeerList(const Peers& peers) = 0;
  virtual void SwitchToStreamingUI() = 0;

  virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video) = 0;
  virtual void StopLocalRenderer() = 0;
  virtual void StartRemoteRenderer(
      webrtc::VideoTrackInterface* remote_video) = 0;
  virtual void StopRemoteRenderer() = 0;

  virtual void QueueUIThreadCallback(int msg_id, void* data) = 0;
};

#ifdef WIN32

class MainWnd : public MainWindow, public vi::IRoomObserver, /*public vi::IParticipantEventHandler, */public std::enable_shared_from_this<MainWnd>
{
 public:
  static const wchar_t kClassName[];

  enum WindowMessages {
    UI_THREAD_CALLBACK = WM_APP + 1,
  };
  enum RenderType {
      LocalRender = 0,
      RemoteRender = 1,
  };
  //MainWnd(const char* server, int port, bool auto_connect, bool auto_call);
  MainWnd();
  ~MainWnd();

  bool Create();
  int JoinRoom(std::string url);
  bool Destroy();
  bool PreTranslateMessage(MSG* msg);
  bool Is_Close() { return exit_; }

  virtual void RegisterObserver(MainWndCallback* callback);
  virtual bool IsWindow();
  virtual void SwitchToConnectUI();
  virtual void SwitchToPeerList(const Peers& peers);
  virtual void SwitchToStreamingUI();
  virtual void MessageBox(const char* caption, const char* text, bool is_error);
  virtual UI current_ui() { return ui_; }

  virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video);
  virtual void StopLocalRenderer();
  virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video);
  virtual void StopRemoteRenderer();
  virtual void AddRender(RenderType type, webrtc::VideoTrackInterface* local_video);

  virtual void QueueUIThreadCallback(int msg_id, void* data);

  // IRoomClientEventHandler
  virtual void onRoomStateChanged(vi::RoomState state);

  virtual void onCreateLocalVideoTrack(const std::string& tid, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>);

  virtual void onRemoveLocalVideoTrack(const std::string& tid, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>);

  virtual void onCreateRemoteVideoTrack(const std::string& tid, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track);

  virtual void onRemoveRemoteVideoTrack(const std::string& tid, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track);

  virtual void onLocalAudioStateChanged(bool enabled, bool muted);

  virtual void onLocalVideoStateChanged(bool enabled);

  virtual void onLocalActiveSpeaker(int32_t volume);

  /*virtual void onParticipantJoin(std::shared_ptr<vi::IParticipant> participant);

  virtual void onParticipantLeave(std::shared_ptr<vi::IParticipant> participant);

  virtual void onRemoteActiveSpeaker(std::shared_ptr<vi::IParticipant> participant, int32_t volume);

  virtual void onDisplayNameChanged(std::shared_ptr<vi::IParticipant> participant);

  virtual void onCreateRemoteVideoTrack(std::shared_ptr<vi::IParticipant> participant, const std::string& tid, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track);

  virtual void onRemoveRemoteVideoTrack(std::shared_ptr<vi::IParticipant> participant, const std::string& tid, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track);

  virtual void onRemoteAudioStateChanged(std::shared_ptr<vi::IParticipant> participant, bool muted);

  virtual void onRemoteVideoStateChanged(std::shared_ptr<vi::IParticipant> participant, bool muted);
*/
  HWND handle() const { return wnd_; }

  class VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
   public:
    VideoRenderer(HWND wnd,
                  int width,
                  int height,
                  webrtc::VideoTrackInterface* track_to_render, 
                  RenderType render_type);
    virtual ~VideoRenderer();

    void Lock() { ::EnterCriticalSection(&buffer_lock_); }

    void Unlock() { ::LeaveCriticalSection(&buffer_lock_); }

    // VideoSinkInterface implementation
    void OnFrame(const webrtc::VideoFrame& frame) override;

    const BITMAPINFO& bmi() const { return bmi_; }
    const uint8_t* image() const { return image_.get(); }
    void WriteYuv(rtc::scoped_refptr<webrtc::I420BufferInterface> buffer);

    void SetRenderRect(const RECT& rect);
    webrtc::VideoTrackInterface* track() {
        return rendered_track_ ? rendered_track_.get() : NULL;
    }
    RECT GetRenderRect();
    RenderType GetRenderType() { return render_type_; }
   protected:
    void SetSize(int width, int height);

    enum {
      SET_SIZE,
      RENDER_FRAME,
    };

    HWND wnd_;
    BITMAPINFO bmi_;
    std::unique_ptr<uint8_t[]> image_;
    CRITICAL_SECTION buffer_lock_;
    rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
    FILE* file_;
    RECT rect_;
    RenderType render_type_;
  };

  // A little helper class to make sure we always to proper locking and
  // unlocking when working with VideoRenderer buffers.
  template <typename T>
  class AutoLock {
   public:
    explicit AutoLock(T* obj) : obj_(obj) { obj_->Lock(); }
    ~AutoLock() { obj_->Unlock(); }

   protected:
    T* obj_;
  };

 protected:
  enum ChildWindowID {
    EDIT_ID = 1,
    BUTTON_ID,
    LABEL1_ID,
    LABEL2_ID,
    LISTBOX_ID,
  };

  void OnPaint();

  void PaintImage(HDC& hdc, /*HDC& dc_mem, */VideoRenderer* render, const RECT& rect);

  void OnDestroyed();

  void OnDefaultAction();

  bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result);

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static bool RegisterWindowClass();

  void CreateChildWindow(HWND* wnd,
                         ChildWindowID id,
                         const wchar_t* class_name,
                         DWORD control_style,
                         DWORD ex_style);
  void CreateChildWindows();

  void LayoutConnectUI(bool show);
  void LayoutPeerListUI(bool show);

  void HandleTabbing();
 
 private:
  //std::unique_ptr<VideoRenderer> local_renderer_;
  //std::unique_ptr<VideoRenderer> remote_renderer_;
  UI ui_;
  HWND wnd_;
  DWORD ui_thread_id_;
  HWND edit1_;
  HWND edit2_;
  HWND label1_;
  HWND label2_;
  HWND button_;
  HWND listbox_;
  bool destroyed_;
  void* nested_msg_;
  MainWndCallback* callback_;
  static ATOM wnd_class_;
  std::string url_;
  std::string port_;
  bool auto_connect_;
  bool auto_call_;
  std::shared_ptr<vi::IMediaControl> media_ctrl_;
  //rtc::Thread* _callbackThread;
  std::vector< std::shared_ptr<VideoRenderer>> render_list_;
  std::vector<RECT> rect_list_;
  bool exit_;
};
#endif  // WIN32

#endif  // EXAMPLES_PEERCONNECTION_CLIENT_MAIN_WND_H_
