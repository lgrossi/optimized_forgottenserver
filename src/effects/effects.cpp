#include "effects.hpp"

Effects::Effects()
{
	m_effect.effect = new Effect;
}

Effects::~Effects()
{
	delete m_effect.effect;
}

void Effects::reload()
{
	effectsJSON.clear();
	load();
}

void Effects::load()
{
		// cache effects JSON file
		if (!effectsJSON.is_object()) {
			effectsJSON = JSONParser::openFile(Effects::JSON_FILE_NAME);
		}
}

Effect* Effects::getByID(std::string id)
{
	if (m_effect.id == id) return m_effect.effect;

	if (!JSONParser::isKeySafe(effectsJSON, id)) {
		std::cout << "ERROR: Malformed effect attributes - effect must be an object with valid id!" << std::endl;
		return nullptr;
	}

	json effectJSON = effectsJSON[id];
	if (!effectJSON.is_array() || effectJSON.size() <= 0) {
		std::cout << "ERROR: Malformed effect with ID: " << id << " - must be a populated array!" << std::endl;
		return nullptr;
	}

	m_effect.id = id;
	m_effect.effect->clear();

	for (auto const& effectPieceJSON : effectJSON) {
		if (!JSONParser::isKeySafe(effectPieceJSON, "id") || !effectPieceJSON["id"].is_number()) {
			std::cout << "ERROR: Invalid effectPiece id - id must be an uint16_t!" << std::endl;
			return nullptr;
		}

		if (!JSONParser::isKeySafe(effectPieceJSON, "childrenIds") || !effectPieceJSON["childrenIds"].is_array()) {
			std::cout << "ERROR: Invalid effectPiece childrenIds - childrenIds must be an array!" << std::endl;
			return nullptr;
		}

		if (!JSONParser::isKeySafe(effectPieceJSON, "offset") || !effectPieceJSON["offset"].is_object()) {
			std::cout << "ERROR: Invalid effectPiece offset - offset must be an object {x, y, z}!" << std::endl;
			return nullptr;
		}
		m_effect.effect->emplace_back(EffectPiece{
			effectPieceJSON["id"],
			getOffsetPos(effectPieceJSON["offset"]),
			effectPieceJSON["childrenIds"]
		});
	}

	return m_effect.effect;
}

Position Effects::getOffsetPos(json offset) {
	offset["x"] = offset["x"].is_number() ? offset["x"] : "0"_json;
	offset["y"] = offset["y"].is_number() ? offset["y"] : "0"_json;
	offset["z"] = offset["z"].is_number() ? offset["z"] : "0"_json;

	return Position(offset["x"], offset["y"], offset["z"]);
};
