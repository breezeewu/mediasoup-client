#include "peer_connection_client.h"
#include "mssignal/msapi.h"
#include "sdputil/RemoteSdp.hpp"
#include "IRoomObserver.h"
#include <mutex>
//#define ENABLE_AUTO_SUBSCRIBE
/*class IMediaControl
{
public:
	virtual ~IMediaControl() {}

	virtual int join(const std::string& url, user_info* puser_info = NULL) = 0;

	virtual void leave() = 0;

	virtual int push(bool benable_video, bool benable_audio) = 0;

	virtual int unpublish() = 0;

	virtual int subscribe(std::string uid) = 0;

	virtual int unsubscribe(std::string uid) = 0;
};*/

class stream_info
{
public:
	string												id;
	rtc::scoped_refptr<webrtc::RtpTransceiverInterface>	transciever;
	std::shared_ptr<mediasoupclient::Sdp::RemoteSdp>	remote_sdp;
};

class MediaControl:public vi::IMediaControl, public PrivateListener, public std::enable_shared_from_this<MediaControl>
{
public:
	MediaControl();

	~MediaControl();

	void setObserver(std::shared_ptr<vi::IRoomObserver> room_observer);

	int join(const std::string& url, vi::peer_info* peer_info = NULL);

	void leave();

	int publish(vi::PublishStreamType stream_type, bool benable_audio = true);

	int unpublish(vi::PublishStreamType stream_type);

	int subscribe(std::string uid);

	int unsubscribe(std::string uid);

	//PrivateListener implement
	void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState) override;
	void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
	void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
	void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel) override;
	void OnRenegotiationNeeded() override;
	void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState) override;
	void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState newState) override;
	void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
	void OnIceCandidatesRemoved(const std::vector<cricket::Candidate>& candidates) override;
	void OnIceConnectionReceivingChange(bool receiving) override;
	void OnAddTrack(
		rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
		const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override;
	void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override;
	void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
	void OnInterestingUsage(int usagePattern) override;

	void on_new_consumer(string req);

	void on_new_data_consumer(string req);
	//void configVideoEncodings();

protected:
	int init();

	void deinit();

	int enable_video(std::unique_ptr<PeerConnectionClient>& pc, vi::PublishStreamType stream_type, bool enable);

	int enable_audio(std::unique_ptr<PeerConnectionClient>& pc, bool enable);
	
	string create_remote_sdp();

	string create_local_sdp();

	void configVideoEncodings();

	int subscribe_stream(shared_ptr<signaling::NewConsumerRequest> con_req);

private:
	map<vi::PublishStreamType, std::unique_ptr<PeerConnectionClient>> publish_pcs_;
	map<string, std::unique_ptr<PeerConnectionClient>> sub_pcs_;
	std::unique_ptr<PeerConnectionClient>	pcc_;
	std::shared_ptr<MediaSoupAPI>			ms_api_;
	unique_ptr<vi::peer_info>					user_info_;
	std::vector<webrtc::RtpEncodingParameters> encodings_;
	std::unique_ptr<mediasoupclient::Sdp::RemoteSdp> remote_sdp_;
	std::shared_ptr <vi::IRoomObserver>		room_observer_;
	std::map<string, std::shared_ptr<stream_info>>	recv_stream_list_;
	std::map<string, std::shared_ptr<stream_info>>	send_stream_list_;
	unique_ptr<rtc::Thread>					task_queue_;
	map<string, map<string, shared_ptr<signaling::NewConsumerRequest>>>		consumer_list_;
	map<string, shared_ptr<signaling::NewDataConsumerRequest>>				data_consumer_list_;
	map<string, vi::peer_info>				peer_info_list_;
	recursive_mutex							mutex_;
};