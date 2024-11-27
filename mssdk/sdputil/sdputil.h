#ifndef MSC_SDP_UTILS_H_
#define MSC_SDP_UTILS_H_

#include <json/json.hpp>
#include <json/sdptransform.hpp>
#include <api/media_stream_interface.h>
#include <string>
//#include "../../deps/libsdptransform/include/json.hpp"

json extractRtpCapabilities(const json& sdpObject);
json extractDtlsParameters(const json& sdpObject);
void fillRtpParametersForTrack(json& rtpParameters, const json& sdpObject, const std::string& mid);
void addLegacySimulcast(json& offerMediaObject, uint8_t numStreams);
std::string getCname(const json& offerMediaObject);
json getRtpEncodings(const json& offerMediaObject);
void applyCodecParameters(const json& offerRtpParameters, json& answerMediaObject);

std::string getRemoteDtlsRoleFromSDP(const std::string& localDtlsRole, nlohmann::json sdpobj);

json parseScalabilityMode(const std::string& scalabilityMode);

#endif