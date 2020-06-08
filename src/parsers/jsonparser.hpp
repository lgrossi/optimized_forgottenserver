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

    static json readFromFile(std::string fileName) {
			try {
				std::ifstream ifStream(JSONParser::generateFilePath(fileName));
				json j = json::parse(ifStream);
				ifStream.close();
				return j;
			}
			catch(const std::exception& e) {
				std::string str;
				str.reserve(128);
				str.append("Error - JSONParser::readFromFile ")
					.append(JSON_PATH)
					.append(fileName)
					.append(JSON_FORMAT)
					.append(" not found.");
				std::cerr << str << std::endl;
				return json();
			}
		};

		static std::string generateFilePath(std::string fileName) {
			std::string str;
			str.reserve(128);
			return str.append(JSONParser::JSON_PATH).append(fileName).append(JSONParser::JSON_FORMAT);
		}

		static bool isKeySafe(json j, std::string key) {
			return j.is_object() && j.find(key) != j.end();
		}

	private:
		
		
};

#endif
