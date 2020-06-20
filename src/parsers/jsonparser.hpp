#ifndef LG_JSON_PARSER_HPP
#define LG_JSON_PARSER_HPP

#include <string>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class JSONParser
{
	public:
		constexpr JSONParser() = default;
		virtual ~JSONParser() = default;

		static constexpr const char* JSON_PATH = "data/JSON/";
		static constexpr const char* JSON_FORMAT = ".json";

		static std::string generateFilePath(std::string filename, bool addPath = true) {
			std::string str;
			str.reserve(128);
			if (addPath) str.append(JSONParser::JSON_PATH);
			return str.append(filename).append(JSONParser::JSON_FORMAT);
		}

		static bool isKeySafe(json j, std::string key) {
			return j.is_object() && j.find(key) != j.end();
		}

    static json readFromFile(std::string filename, bool addPath = true) {
			const std::string path = JSONParser::generateFilePath(filename, addPath);

			try {
				std::ifstream ifStream(path);
				json j = json::parse(ifStream);
				ifStream.close();
				return j;
			}
			catch(const std::exception& e) {
				std::string str;
				str.reserve(128);
				str.append("Error - JSONParser::readFromFile ")
					.append(path)
					.append(" not found or malformed.");
				std::cerr << str << std::endl;
				return json();
			}
		};

	private:
		
		
};

#endif
