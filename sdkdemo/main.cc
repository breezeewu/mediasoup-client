/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// clang-format off
// clang formating would change include order.
#include <windows.h>
#include <shellapi.h>  // must come after windows.h
// clang-format on

#include <string>
#include <vector>
//#pragma comment(lib, "crash_handler.lib")
//#include "absl/flags/parse.h"
//#include "crash_handler.h"
//#include "examples/peerconnection/client/conductor.h"
//#include "flag_defs.h"
#include "main_wnd.h"
//#include "examples/peerconnection/client/peer_connection_client.h"
#include "rtc_base/checks.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/string_utils.h"  // For ToUtf8
#include "rtc_base/win32_socket_init.h"
#include "rtc_base/win32_socket_server.h"
#include "system_wrappers/include/field_trial.h"
//#include "test/field_trial.h"

#pragma comment(lib, "strmiids.lib")
//#pragma comment(lib, "libssl.lib")
//#pragma comment(lib, "libcrypto.lib")
//#pragma comment(lib, "cpr.lib")
//#pragma comment(lib, "libcurl.lib")
//#pragma comment(lib, "zlib.lib")
//#pragma comment(lib, "opengl32.lib")
//#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "winmm.lib")
// IID_IMediaBuffer,IID_IMediaObject
#pragma comment(lib, "dmoguids.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Secur32.lib")
// CLSID_CWMAudioAEC
#pragma comment(lib, "wmcodecdspuuid.lib")
#pragma comment(lib, "Msdmo.lib")  // MoFreeMediaType
#pragma comment(lib, "msvcrt.lib")
//#pragma comment(lib, "glew32.lib")
namespace {
// A helper class to translate Windows command line arguments into UTF8,
// which then allows us to just pass them to the flags system.
// This encapsulates all the work of getting the command line and translating
// it to an array of 8-bit strings; all you have to do is create one of these,
// and then call argc() and argv().
class WindowsCommandLineArguments {
 public:
  WindowsCommandLineArguments();

  int argc() { return argv_.size(); }
  char** argv() { return argv_.data(); }

 private:
  // Owned argument strings.
  std::vector<std::string> args_;
  // Pointers, to get layout compatible with char** argv.
  std::vector<char*> argv_;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(WindowsCommandLineArguments);
};

WindowsCommandLineArguments::WindowsCommandLineArguments() {
  // start by getting the command line.
  /*LPCWSTR command_line = ::GetCommandLineW();
  // now, convert it to a list of wide char strings.
  int argc;
  LPWSTR* wide_argv = ::CommandLineToArgvW(command_line, &argc);

  // iterate over the returned wide strings;
  for (int i = 0; i < argc; ++i) {
    args_.push_back(rtc::ToUtf8(wide_argv[i], wcslen(wide_argv[i])));
    // make sure the argv array points to the string data.
    argv_.push_back(const_cast<char*>(args_.back().c_str()));
  }
  LocalFree(wide_argv);*/
}

}  // namespace
int PASCAL WinMain(HINSTANCE instance,
                    HINSTANCE prev_instance,
                    char* cmd_line,
                    //wchar_t* cmd_line,
                    int cmd_show) {
  rtc::WinsockInitializer winsock_init;
  rtc::Win32SocketServer w32_ss;
  rtc::Win32Thread w32_thread(&w32_ss);
  rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

  WindowsCommandLineArguments win_args;
  int argc = win_args.argc();
  char** argv = win_args.argv();
  const std::string forced_field_trials =
      "WebRTC-FlexFEC-03-Advertised/Enabled/WebRTC-FlexFEC-03/Enabled/";
  // absl::GetFlag(FLAGS_force_fieldtrials);
  webrtc::field_trial::InitFieldTrialsFromString(forced_field_trials.c_str());
  //absl::ParseCommandLine(argc, argv);
  //InitCrashHandler(L"peepconnect", L"./", CrashFullDumpType);
  // InitFieldTrialsFromString stores the char*, so the char array must outlive
  // the application.
  // constexpr char kFlexFecEnabledFieldTrials[] =
  //"WebRTC-FlexFEC-03/Enabled/WebRTC-FlexFEC-03-Advertised/Enabled";
  //"WebRTC-FlexFEC-03-Advertised/Enabled/WebRTC-FlexFEC-03/Enabled/";
  /*const std::string forced_field_trials =
      "WebRTC-FlexFEC-03-Advertised/Enabled/WebRTC-FlexFEC-03/Enabled/";
  // absl::GetFlag(FLAGS_force_fieldtrials);
  webrtc::field_trial::InitFieldTrialsFromString(forced_field_trials.c_str());*/

  // Abort if the user specifies a port that is outside the allowed
  // range [1, 65535].
  /*if ((absl::GetFlag(FLAGS_port) < 1) || (absl::GetFlag(FLAGS_port) > 65535)) {
    printf("Error: %i is not a valid port.\n", absl::GetFlag(FLAGS_port));
    return -1;
  }*/

  //const std::string server = absl::GetFlag(FLAGS_server);
  //MainWnd wnd(server.c_str(), absl::GetFlag(FLAGS_port), false, false);
              //absl::GetFlag(FLAGS_autoconnect), absl::GetFlag(FLAGS_autocall));
  std::shared_ptr<MainWnd> wnd(new MainWnd());
  if (!wnd->Create()) {
    RTC_NOTREACHED();
    return -1;
  }
  //wnd.SwitchToStreamingUI();
  rtc::InitializeSSL();
  /*PeerConnectionClient client;
  rtc::scoped_refptr<Conductor> conductor(
      new rtc::RefCountedObject<Conductor>(&client, &wnd));*/

  // Main loop.
  MSG msg;
  BOOL gm;
  while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
    if (!wnd->PreTranslateMessage(&msg)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  }

  //if (conductor->connection_active() || client.is_connected()) {
    while (!wnd->Is_Close() && //(conductor->connection_active() || client.is_connected()) &&
           (gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
      if (!wnd->PreTranslateMessage(&msg)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
      }
    }
 // }

  rtc::CleanupSSL();
  return 0;
}
