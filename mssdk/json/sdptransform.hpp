#ifndef SDPTRANSFORM_HPP
#define SDPTRANSFORM_HPP

#include "json.hpp"
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <functional>

using json = nlohmann::json;

namespace sdptransform
{
	namespace grammar
	{
		struct Rule
		{
			std::string name;
			std::string push;
			std::regex reg;
			std::vector<std::string> names;
			std::vector<char> types;
			std::string format;
			std::function<const std::string(const json&)> formatFunc;
		};

		extern const std::map<char, std::vector<Rule>> rulesMap;
	}

	json parse(const std::string& sdp);

	json parseParams(const std::string& str);

	std::vector<int> parsePayloads(const std::string& str);

	json parseImageAttributes(const std::string& str);

	json parseSimulcastStreamList(const std::string& str);

	std::string write(json& session);

	json extractRtpCapabilities(const json& sdpObject);
	json extractDtlsParameters(const json& sdpObject);
	void fillRtpParametersForTrack(json& rtpParameters, const json& sdpObject, const std::string& mid);
	void addLegacySimulcast(json& offerMediaObject, uint8_t numStreams);
	std::string getCname(const json& offerMediaObject);
	json getRtpEncodings(const json& offerMediaObject);
	void applyCodecParameters(const json& offerRtpParameters, json& answerMediaObject);
}

#endif
