#pragma once
#include <string>
#include "api/scoped_refptr.h"

namespace webrtc {
    class MediaStreamTrackInterface;
}

namespace vi {
    enum RoomState {
        UNKNOWN = 0,
        CONNECTING,
        CONNECTED,
        CLOSED
    };
    enum PublishStreamType {
        PublishMainStream = 0,
        publishScreenStream,
    };
    struct peer_info
    {
        std::string room_id;
        std::string display_name;
        std::string peer_id;
        std::string device_flag;
        std::string device_name;
        std::string version;
    };

    class IRoomObserver
    {
    public:
        virtual ~IRoomObserver() {}

        virtual void onRoomStateChanged(RoomState state) = 0;

        virtual void onCreateLocalVideoTrack(const std::string& tid, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>) = 0;

        virtual void onRemoveLocalVideoTrack(const std::string& tid, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>) = 0;

        virtual void onCreateRemoteVideoTrack(const std::string& tid, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>) = 0;

        virtual void onRemoveRemoteVideoTrack(const std::string& tid, rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>) = 0;

        virtual void onLocalAudioStateChanged(bool enabled, bool muted) = 0;

        virtual void onLocalVideoStateChanged(bool enabled) = 0;

        virtual void onLocalActiveSpeaker(int32_t volume) = 0;
    };

    class IMediaControl
    {
    public:
        virtual ~IMediaControl() {}

        virtual int join(const std::string& url, peer_info* peer_info = NULL) = 0;

        virtual void leave() = 0;

        virtual int publish(PublishStreamType stream_type, bool benable_audio) = 0;

        virtual int unpublish(PublishStreamType stream_type) = 0;

        virtual int subscribe(std::string uid) = 0;

        virtual int unsubscribe(std::string uid) = 0;
    };
};

std::shared_ptr<vi::IMediaControl> create_media_control(std::shared_ptr<vi::IRoomObserver> room_observer);