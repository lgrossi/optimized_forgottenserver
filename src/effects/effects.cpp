#include "effects.hpp"

std::string Effect::toString() {
	std::string str;
	str.reserve(128);

	str.append("Effect '")
		.append(std::to_string(getId()))
		.append("' has ")
		.append(std::to_string(getPieces().size()))
		.append(" pieces")
		.append("\n");

	for (auto const& effectPiece : getPieces()) {
		str.append("\nPiece: ")
			.append(std::to_string(effectPiece.id))
			.append("\nOffset: ")
			.append("x: ").append(std::to_string(effectPiece.offset.x))
			.append(" y: ").append(std::to_string(effectPiece.offset.y))
			.append(" z: ").append(std::to_string(effectPiece.offset.z))
			.append("\nChildren: [ ");
		
		for (auto const& childId : effectPiece.childrenIds) {
			str.append(std::to_string(childId)).append(" ");
		}

		str.append("]\n");
	}
	return str;
}

Effects::Effects()
{
	m_effect = new Effect;
}

Effects::~Effects()
{
	delete m_effect;
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
			effectsJSON = JSONParser::readFromFile(Effects::JSON_FILE_NAME);
		}
}

Effect* Effects::getByID(uint16_t id)
{
	if (m_effect->getId() == id) return m_effect;

	std::string strId = std::to_string(id);
	if (!JSONParser::isKeySafe(effectsJSON, strId)) {
		std::cout << "ERROR: Malformed effect attributes - effect must be an object with valid id!" << std::endl;
		return nullptr;
	}

	json effectJSON = effectsJSON[strId];
	if (!effectJSON.is_array() || effectJSON.size() <= 0) {
		std::cout << "ERROR: Malformed effect with ID: " << strId << " - must be a populated array!" << std::endl;
		return nullptr;
	}

	m_effect->setId(id);
	m_effect->clearPieces();
	std::cout << m_effect->getId() << std::endl;

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
		m_effect->addPiece(EffectPiece{
			effectPieceJSON["id"],
			getOffsetPos(effectPieceJSON["offset"]),
			effectPieceJSON["childrenIds"]
		});
	}

	return m_effect;
}

Position Effects::getOffsetPos(json offset) {
	offset["x"] = offset["x"].is_number() ? offset["x"] : "0"_json;
	offset["y"] = offset["y"].is_number() ? offset["y"] : "0"_json;
	offset["z"] = offset["z"].is_number() ? offset["z"] : "0"_json;

	return Position(offset["x"], offset["y"], offset["z"]);
};
