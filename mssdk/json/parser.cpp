#include "sdptransform.hpp"
#include <unordered_map>
#include <cstddef>   // size_t
#include <memory>    // std::addressof()
#include <sstream>   // std::stringstream, std::istringstream
#include <ios>       // std::noskipws
#include <algorithm> // std::find_if()
#include <cctype>    // std::isspace()
#include <cstdint>   // std::uint64_t
#include "Utils.hpp"
#include "lazyutil/lazyexception.hpp"

namespace sdptransform
{
	void parseReg(const grammar::Rule& rule, json& location, const std::string& content);

	void attachProperties(
		const std::smatch& match,
		json& location,
		const std::vector<std::string>& names,
		const std::string& rawName,
		const std::vector<char>& types
	);

	json toType(const std::string& str, char type);

	bool isNumber(const std::string& str);

	bool isInt(const std::string& str);

	bool isFloat(const std::string& str);

	void trim(std::string& str);

	void insertParam(json& o, const std::string& str);

	json parse(const std::string& sdp)
	{
		static const std::regex ValidLineRegex("^([a-z])=(.*)");

		json session = json::object();
		std::stringstream sdpstream(sdp);
		std::string line;
		json media = json::array();
		json* location = std::addressof(session);

		while (std::getline(sdpstream, line, '\n'))
		{
			// Remove \r if lines are separated with \r\n (as mandated in SDP).
			if (line.size() && line[line.length() - 1] == '\r')
				line.pop_back();

			// Ensure it's a valid SDP line.
			if (!std::regex_search(line, ValidLineRegex))
				continue;

			char type = line[0];
			std::string content = line.substr(2);

			if (type == 'm')
			{
				json m = json::object();

				m["rtp"] = json::array();
				m["fmtp"] = json::array();

				media.push_back(m);

				// Point at latest media line.
				location = std::addressof(media[media.size() - 1]);
			}

			auto it = grammar::rulesMap.find(type);

			if (it == grammar::rulesMap.end())
				continue;

			auto& rules = it->second;

			for (size_t j = 0; j < rules.size(); ++j)
			{
				auto& rule = rules[j];

				if (std::regex_search(content, rule.reg))
				{
					parseReg(rule, *location, content);

					break;
				}
			}
		}

		// Link it up.
		session["media"] = media;

		return session;
	}

	json parseParams(const std::string& str)
	{
		json obj = json::object();
		std::stringstream ss(str);
		std::string param;

		while (std::getline(ss, param, ';'))
		{
			trim(param);

			if (param.length() == 0)
				continue;

			insertParam(obj, param);
		}

		return obj;
	}

	std::vector<int> parsePayloads(const std::string& str)
	{
		std::vector<int> arr;

		std::stringstream ss(str);
		std::string payload;

		while (std::getline(ss, payload, ' '))
		{
			arr.push_back(std::stoi(payload));
		}

		return arr;
	}

	json parseImageAttributes(const std::string& str)
	{
		json arr = json::array();
		std::stringstream ss(str);
		std::string item;

		while (std::getline(ss, item, ' '))
		{
			trim(item);

			// Special case for * value.
			if (item == "*")
				return item;

			if (item.length() < 5) // [x=0]
				continue;

			json obj = json::object();
			std::stringstream ss2(item.substr(1, item.length() - 2));
			std::string param;

			while (std::getline(ss2, param, ','))
			{
				trim(param);

				if (param.length() == 0)
					continue;

				insertParam(obj, param);
			}

			arr.push_back(obj);
		}

		return arr;
	}

	json parseSimulcastStreamList(const std::string& str)
	{
		json arr = json::array();
		std::stringstream ss(str);
		std::string item;

		while (std::getline(ss, item, ';'))
		{
			if (item.length() == 0)
				continue;

			json arr2 = json::array();
			std::stringstream ss2(item);
			std::string format;

			while (std::getline(ss2, format, ','))
			{
				if (format.length() == 0)
					continue;

				json obj = json::object();

				if (format[0] != '~')
				{
					obj["scid"] = format;
					obj["paused"] = false;
				}
				else
				{
					obj["scid"] = format.substr(1);
					obj["paused"] = true;
				}

				arr2.push_back(obj);
			}

			arr.push_back(arr2);
		}

		return arr;
	}

	void parseReg(const grammar::Rule& rule, json& location, const std::string& content)
	{
		bool needsBlank = !rule.name.empty() && !rule.names.empty();

		if (!rule.push.empty() && location.find(rule.push) == location.end())
		{
			location[rule.push] = json::array();
		}
		else if (needsBlank && location.find(rule.name) == location.end())
		{
			location[rule.name] = json::object();
		}

		std::smatch match;

		std::regex_search(content, match, rule.reg);

		json object = json::object();
		json& keyLocation = !rule.push.empty()
			// Blank object that will be pushed.
			? object
			// Otherwise named location or root.
			: needsBlank
				? location[rule.name]
				: location;

		attachProperties(match, keyLocation, rule.names, rule.name, rule.types);

		if (!rule.push.empty())
			location[rule.push].push_back(keyLocation);
	}

	void attachProperties(
		const std::smatch& match,
		json& location,
		const std::vector<std::string>& names,
		const std::string& rawName,
		const std::vector<char>& types
	)
	{
		if (!rawName.empty() && names.empty())
		{
			location[rawName] = toType(match[1].str(), types[0]);
		}
		else
		{
			for (size_t i = 0; i < names.size(); ++i)
			{
				if (i + 1 < match.size() && !match[i + 1].str().empty())
				{
					location[names[i]] = toType(match[i + 1].str(), types[i]);
				}
			}
		}
	}

	bool isInt(const std::string& str)
	{
		std::istringstream iss(str);
		long l;

		iss >> std::noskipws >> l;

		return iss.eof() && !iss.fail();
	}

	bool isFloat(const std::string& str)
	{
		std::istringstream iss(str);
		float f;

		iss >> std::noskipws >> f;

		return iss.eof() && !iss.fail();
	}

	json toType(const std::string& str, char type)
	{
		// https://stackoverflow.com/a/447307/4827838.

		switch (type)
		{
			case 's':
			{
				return str;
			}

			case 'u':
			{
				std::istringstream iss(str);
				std::uint64_t ll;

				iss >> std::noskipws >> ll;

				if (iss.eof() && !iss.fail())
					return ll;
				else
					return 0u;
			}

			case 'd':
			{
				std::istringstream iss(str);
				std::int64_t ll;

				iss >> std::noskipws >> ll;

				if (iss.eof() && !iss.fail())
					return ll;
				else
					return 0;
			}

			case 'f':
			{
				std::istringstream iss(str);
				double d;

				iss >> std::noskipws >> d;

				if (iss.eof() && !iss.fail())
					return std::stod(str);
				else
					return 0.0f;
			}
		}

		return nullptr;
	}

	void trim(std::string& str)
	{
		str.erase(
			str.begin(),
			std::find_if(
				str.begin(), str.end(), [](unsigned char ch) { return !std::isspace(ch); }
			)
		);

		str.erase(
			std::find_if(
				str.rbegin(), str.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(),
			str.end()
		);
	}

	// @str parameters is a string like "profile-level-id=42e034".
	void insertParam(json& o, const std::string& str)
	{
		static const std::regex KeyValueRegex("^\\s*([^= ]+)(?:\\s*=\\s*([^ ]+))?$");
		static const std::unordered_map<std::string, char> WellKnownParameters =
		{
			// H264 codec parameters.
			{ "profile-level-id",   's' },
			{ "packetization-mode", 'd' },
			// VP9 codec parameters.
			{ "profile-id",         's' }
		};

		std::smatch match;

		std::regex_match(str, match, KeyValueRegex);

		if (match.size() == 0)
			return;

		std::string param = match[1].str();
		std::string value = match[2].str();

		auto it = WellKnownParameters.find(param);
		char type;

		if (it != WellKnownParameters.end())
			type = it->second;
		else if (isInt(match[2].str()))
			type = 'd';
		else if (isFloat(match[2].str()))
			type = 'f';
		else
			type = 's';

		// Insert into the given JSON object.
		o[match[1].str()] = toType(match[2].str(), type);
	}

	json extractRtpCapabilities(const json& sdpObject)
	{
		//MSC_TRACE();

		// Map of RtpCodecParameters indexed by payload type.
		std::map<uint8_t, json> codecsMap;

		// Array of RtpHeaderExtensions.
		auto headerExtensions = json::array();

		// Whether a m=audio/video section has been already found.
		bool gotAudio = false;
		bool gotVideo = false;

		for (const auto& m : sdpObject["media"])
		{
			auto kind = m["type"].get<std::string>();

			if (kind == "audio")
			{
				if (gotAudio)
					continue;

				gotAudio = true;
			}
			else if (kind == "video")
			{
				if (gotVideo)
					continue;

				gotVideo = true;
			}
			else
			{
				continue;
			}

			// Get codecs.
			for (const auto& rtp : m["rtp"])
			{
				std::string mimeType(kind);
				mimeType.append("/").append(rtp["codec"].get<std::string>());

				// clang-format off
				json codec =
				{
					{ "kind",                 kind           },
					{ "mimeType",             mimeType       },
					{ "preferredPayloadType", rtp["payload"] },
					{ "clockRate",            rtp["rate"]    },
					{ "parameters",           json::object() },
					{ "rtcpFeedback",         json::array()  }
				};
				// clang-format on

				if (kind == "audio")
				{
					auto jsonEncodingIt = rtp.find("encoding");

					if (jsonEncodingIt != rtp.end() && jsonEncodingIt->is_string())
						codec["channels"] = std::stoi(jsonEncodingIt->get<std::string>());
					else
						codec["channels"] = 1;
				}

				codecsMap[codec["preferredPayloadType"].get<uint8_t>()] = codec;
			}

			// Get codec parameters.
			for (const auto& fmtp : m["fmtp"])
			{
				auto parameters = sdptransform::parseParams(fmtp["config"]);
				auto jsonPayloadIt = codecsMap.find(fmtp["payload"]);

				if (jsonPayloadIt == codecsMap.end())
					continue;

				// If preset, convert 'profile-id' parameter (VP8 and VP9) into
				// integer since we define it that way in mediasoup RtpCodecParameters
				// and RtpCodecCapability.
				auto profileIdIt = parameters.find("profile-id");

				if (profileIdIt != parameters.end() && profileIdIt->is_string())
					parameters["profile-id"] = std::stoi(profileIdIt->get<std::string>());

				auto& codec = jsonPayloadIt->second;

				codec["parameters"] = parameters;
			}

			// Get RTCP feedback for each codec.
			for (const auto& fb : m["rtcpFb"])
			{
				auto jsonCodecIt = codecsMap.find(std::stoi(fb["payload"].get<std::string>()));

				if (jsonCodecIt == codecsMap.end())
					continue;

				auto& codec = jsonCodecIt->second;

				// clang-format off
				json feedback =
				{
					{"type", fb["type"]}
				};
				// clang-format on

				auto jsonSubtypeIt = fb.find("subtype");

				if (jsonSubtypeIt != fb.end())
					feedback["parameter"] = *jsonSubtypeIt;

				codec["rtcpFeedback"].push_back(feedback);
			}

			// Get RTP header extensions.
			for (const auto& ext : m["ext"])
			{
				// clang-format off
				json headerExtension =
				{
						{ "kind",        kind },
						{ "uri",         ext["uri"] },
						{ "preferredId", ext["value"] }
				};
				// clang-format on

				headerExtensions.push_back(headerExtension);
			}
		}

		// clang-format off
		json rtpCapabilities =
		{
			{ "headerExtensions", headerExtensions },
			{ "codecs",           json::array() },
			{ "fecMechanisms",    json::array() } // TODO
		};
		// clang-format on

		for (auto& kv : codecsMap)
		{
			rtpCapabilities["codecs"].push_back(kv.second);
		}

		return rtpCapabilities;
	}

	json extractDtlsParameters(const json& sdpObject)
	{
		//MSC_TRACE();

		json m;
		json fingerprint;
		std::string role;

		for (const auto& media : sdpObject["media"])
		{
			if (media.find("iceUfrag") != media.end() && media["port"] != 0)
			{
				m = media;
				break;
			}
		}

		if (m.find("fingerprint") != m.end())
			fingerprint = m["fingerprint"];
		else if (sdpObject.find("fingerprint") != sdpObject.end())
			fingerprint = sdpObject["fingerprint"];

		if (m.find("setup") != m.end())
		{
			std::string setup = m["setup"];

			if (setup == "active")
				role = "client";
			else if (setup == "passive")
				role = "server";
			else if (setup == "actpass")
				role = "auto";
		}

		// clang-format off
		json dtlsParameters =
		{
			{ "role",         role },
			{ "fingerprints",
				{
					{
						{ "algorithm", fingerprint["type"] },
						{ "value",     fingerprint["hash"] }
					}
				}
			}
		};
		// clang-format on

		return dtlsParameters;
	}

	void addLegacySimulcast(json& offerMediaObject, uint8_t numStreams)
	{
		//MSC_TRACE();

		if (numStreams <= 1)
			return;

		// Get the SSRC.
		auto mSsrcs = offerMediaObject["ssrcs"];

		auto jsonSsrcIt = std::find_if(mSsrcs.begin(), mSsrcs.end(), [](const json& line) {
			auto jsonAttributeIt = line.find("attribute");

			if (jsonAttributeIt == line.end() || !jsonAttributeIt->is_string())
				return false;

			return jsonAttributeIt->get<std::string>() == "msid";
			});

		if (jsonSsrcIt == mSsrcs.end())
		{
			LAZY_THROW_EXCEPTION("a=ssrc line with msid information not found");
		}

		auto& ssrcMsidLine = *jsonSsrcIt;
		auto v = mediasoupclient::Utils::split(ssrcMsidLine["value"].get<std::string>(), ' ');
		auto& streamId = v[0];
		auto& trackId = v[1];
		auto firstSsrc = ssrcMsidLine["id"].get<std::uint32_t>();
		uint32_t firstRtxSsrc{ 0 };

		// Get the SSRC for RTX.

		auto jsonSsrcGroupsIt = offerMediaObject.find("ssrcGroups");

		if (jsonSsrcGroupsIt != offerMediaObject.end())
		{
			auto& ssrcGroups = *jsonSsrcGroupsIt;

			std::find_if(
				ssrcGroups.begin(), ssrcGroups.end(), [&firstSsrc, &firstRtxSsrc](const json& line) {
					auto jsonSemanticsIt = line.find("semantics");

					if (jsonSemanticsIt == line.end() || !jsonSemanticsIt->is_string())
						return false;

					auto jsonSsrcsIt = line.find("ssrcs");

					if (jsonSsrcsIt == line.end() || !jsonSsrcsIt->is_string())
						return false;

					auto v = mediasoupclient::Utils::split(jsonSsrcsIt->get<std::string>(), ' ');

					if (std::stoull(v[0]) == firstSsrc)
					{
						firstRtxSsrc = std::stoull(v[1]);

						return true;
					}

					return false;
				});
		}

		jsonSsrcIt = std::find_if(mSsrcs.begin(), mSsrcs.end(), [](const json& line) {
			auto jsonAttributeIt = line.find("attribute");
			if (jsonAttributeIt == line.end() || !jsonAttributeIt->is_string())
				return false;

			auto jsonIdIt = line.find("id");
			if (jsonIdIt == line.end() || !jsonIdIt->is_number())
				return false;

			return (jsonAttributeIt->get<std::string>() == "cname");
			});

		if (jsonSsrcIt == mSsrcs.end())
			LAZY_THROW_EXCEPTION("CNAME line not found");

		auto cname = (*jsonSsrcIt)["value"].get<std::string>();
		auto ssrcs = json::array();
		auto rtxSsrcs = json::array();

		for (uint8_t i = 0; i < numStreams; ++i)
		{
			ssrcs.push_back(firstSsrc + i);

			if (firstRtxSsrc != 0u)
				rtxSsrcs.push_back(firstRtxSsrc + i);
		}

		offerMediaObject["ssrcGroups"] = json::array();
		offerMediaObject["ssrcs"] = json::array();

		std::vector<uint32_t> ussrcs = ssrcs;
		auto ssrcsLine = mediasoupclient::Utils::join(ussrcs, ' ');

		std::string msidValue(streamId);
		msidValue.append(" ").append(trackId);

		// clang-format off
		offerMediaObject["ssrcGroups"].push_back(
			{
				{ "semantics", "SIM"     },
				{ "ssrcs",     ssrcsLine },
			});
		// clang-format on

		for (auto& i : ssrcs)
		{
			auto ssrc = i.get<uint32_t>();

			// clang-format off
			offerMediaObject["ssrcs"].push_back(
				{
					{ "id",        ssrc    },
					{ "attribute", "cname" },
					{ "value",     cname   }
				});
			// clang-format on

			// clang-format off
			offerMediaObject["ssrcs"].push_back(
				{
					{ "id",        ssrc      },
					{ "attribute", "msid"    },
					{ "value",     msidValue }
				});
			// clang-format on
		}

		for (size_t i = 0; i < rtxSsrcs.size(); ++i)
		{
			auto ssrc = ssrcs[i].get<uint32_t>();
			auto rtxSsrc = rtxSsrcs[i].get<uint32_t>();

			std::string fid;
			fid = std::to_string(ssrc).append(" ");
			fid.append(std::to_string(rtxSsrc));

			// clang-format off
			offerMediaObject["ssrcGroups"].push_back(
				{
					{ "semantics", "FID" },
					{ "ssrcs",     fid   }
				});
			// clang-format on

			// clang-format off
			offerMediaObject["ssrcs"].push_back(
				{
					{ "id",        rtxSsrc },
					{ "attribute", "cname" },
					{ "value",     cname   }
				});
			// clang-format on

			// clang-format off
			offerMediaObject["ssrcs"].push_back(
				{
					{ "id",        rtxSsrc   },
					{ "attribute", "msid"    },
					{ "value",     msidValue }
				});
			// clang-format on
		}
	};

	std::string getCname(const json& offerMediaObject)
	{
		//MSC_TRACE();

		auto jsonMssrcsIt = offerMediaObject.find("ssrcs");

		if (jsonMssrcsIt == offerMediaObject.end())
			return "";

		const json& mSsrcs = *jsonMssrcsIt;

		auto jsonSsrcIt = find_if(mSsrcs.begin(), mSsrcs.end(), [](const json& line) {
			auto jsonAttributeIt = line.find("attribute");

			return (jsonAttributeIt != line.end() && jsonAttributeIt->is_string());
			});

		if (jsonSsrcIt == mSsrcs.end())
			return "";

		const auto& ssrcCnameLine = *jsonSsrcIt;

		return ssrcCnameLine["value"].get<std::string>();
	}

	json getRtpEncodings(const json& offerMediaObject)
	{
		std::list<uint32_t> ssrcs;

		for (const auto& line : offerMediaObject["ssrcs"])
		{
			auto ssrc = line["id"].get<uint32_t>();
			ssrcs.push_back(ssrc);
		}

		if (ssrcs.empty())
			LAZY_THROW_EXCEPTION("no a=ssrc lines found");

		ssrcs.unique();

		// Get media and RTX SSRCs.

		std::map<uint32_t, uint32_t> ssrcToRtxSsrc;

		auto jsonSsrcGroupsIt = offerMediaObject.find("ssrcGroups");
		if (jsonSsrcGroupsIt != offerMediaObject.end())
		{
			const auto& ssrcGroups = *jsonSsrcGroupsIt;

			// First assume RTX is used.
			for (const auto& line : ssrcGroups)
			{
				if (line["semantics"].get<std::string>() != "FID")
					continue;

				auto fidLine = line["ssrcs"].get<std::string>();
				auto v = mediasoupclient::Utils::split(fidLine, ' ');
				auto ssrc = std::stoull(v[0]);
				auto rtxSsrc = std::stoull(v[1]);

				// Remove the RTX SSRC from the List so later we know that they
				// are already handled.
				ssrcs.remove(rtxSsrc);

				// Add to the map.
				ssrcToRtxSsrc[ssrc] = rtxSsrc;
			}
		}

		// Fill RTP parameters.

		auto encodings = json::array();

		for (auto& ssrc : ssrcs)
		{
			json encoding = { { "ssrc", ssrc } };

			auto it = ssrcToRtxSsrc.find(ssrc);

			if (it != ssrcToRtxSsrc.end())
			{
				encoding["rtx"] = { { "ssrc", it->second } };
			}

			encodings.push_back(encoding);
		}

		return encodings;
	}

	void applyCodecParameters(const json& offerRtpParameters, json& answerMediaObject)
	{
		//MSC_TRACE();

		for (const auto& codec : offerRtpParameters["codecs"])
		{
			auto mimeType = codec["mimeType"].get<std::string>();

			std::transform(mimeType.begin(), mimeType.end(), mimeType.begin(), ::tolower);

			// Avoid parsing codec parameters for unhandled codecs.
			if (mimeType != "audio/opus")
				continue;

			auto& rtps = answerMediaObject["rtp"];
			auto jsonRtpIt = find_if(rtps.begin(), rtps.end(), [&codec](const json& r) {
				return r["payload"] == codec["payloadType"];
				});

			if (jsonRtpIt == rtps.end())
				continue;

			// Just in case.
			if (answerMediaObject.find("fmtp") == answerMediaObject.end())
				answerMediaObject["fmtp"] = json::array();

			auto& fmtps = answerMediaObject["fmtp"];
			auto jsonFmtpIt = find_if(fmtps.begin(), fmtps.end(), [&codec](const json& f) {
				return f["payload"] == codec["payloadType"];
				});

			if (jsonFmtpIt == fmtps.end())
			{
				json fmtp = { { "payload", codec["payloadType"] }, { "config", "" } };

				answerMediaObject["fmtp"].push_back(fmtp);
				jsonFmtpIt = answerMediaObject["fmtp"].end() - 1;
			}

			json& fmtp = *jsonFmtpIt;
			json parameters = sdptransform::parseParams(fmtp["config"]);

			if (mimeType == "audio/opus")
			{
				auto jsonSpropStereoIt = codec["parameters"].find("sprop-stereo");

				if (jsonSpropStereoIt != codec["parameters"].end() && jsonSpropStereoIt->is_boolean())
				{
					auto spropStereo = jsonSpropStereoIt->get<bool>();

					parameters["stereo"] = spropStereo ? 1 : 0;
				}
			}

			// Write the codec fmtp.config back.
			std::ostringstream config;

			for (auto& item : parameters.items())
			{
				if (!config.str().empty())
					config << ";";

				config << item.key();
				config << "=";
				if (item.value().is_string())
					config << item.value().get<std::string>();
				else if (item.value().is_number_float())
					config << item.value().get<float>();
				else if (item.value().is_number())
					config << item.value().get<int>();
			}

			fmtp["config"] = config.str();
		}
	}
}
