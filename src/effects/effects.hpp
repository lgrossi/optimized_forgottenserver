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

struct _Effect
{
	std::string id;
	std::vector<EffectPiece>* effect;
};

using Effect = std::vector<EffectPiece>;

class Effects
{
	public:
		Effects();
		~Effects();

		static constexpr const char* JSON_FILE_NAME = "effects";

		void reload();
		void load();
		std::string toString(Effect effect);
		std::string toString(std::string id);
		Effect* getByID(std::string id);
		Position getOffsetPos(json offset);
		
		const json& getEffectsJSON() const {
			return effectsJSON;
		}

	private:
		json effectsJSON;
		_Effect m_effect;
};

#endif
