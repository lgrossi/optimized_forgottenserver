#ifndef LG_EFFECT_HPP
#define LG_EFFECT_HPP

#include "../otpch.h"
#include "../position.h"
#include "../parsers/jsonparser.hpp"

struct EffectPiece
{
	int16_t id;
	Position offset;
	std::vector<uint16_t> childrenIds;
};
using Pieces = std::vector<EffectPiece>;

class Effect
{
	public:
		uint16_t getId() { return id; }
		void setId(uint16_t newId) { id = newId; }

		Pieces getPieces() { return pieces; }
		void addPiece(EffectPiece piece) { pieces.emplace_back(piece); }
		void clearPieces() { pieces.clear(); }

		std::string toString();
	
	private:
		uint16_t id;
		Pieces pieces;
};

class Effects
{
	public:
		Effects();
		~Effects();

		static constexpr const char* JSON_FILE_NAME = "effects";
		const json& getEffectsJSON() const {
			return effectsJSON;
		}

		Effect* getByID(uint16_t id);
		Position getOffsetPos(json offset);
		void load();
		void loadFromPath(std::string path);
		void reload();

	private:
		bool initialized();

		json effectsJSON;
		Effect* m_effect;
};

#endif
