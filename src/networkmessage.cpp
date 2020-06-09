/**
 * The Forgotten Server - a free and open-source MMORPG server emulator
 * Copyright (C) 2020  Mark Samman <mark.samman@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "otpch.h"

#include "networkmessage.h"

#include "container.h"
#include "creature.h"

std::string NetworkMessage::getString(uint16_t stringLen/* = 0*/)
{
	if (stringLen == 0) {
		stringLen = get<uint16_t>();
	}

	if (!canRead(stringLen)) {
		return std::string();
	}

	char* v = reinterpret_cast<char*>(buffer) + info.position; //does not break strict aliasing
	info.position += stringLen;
	return std::string(v, stringLen);
}

Position NetworkMessage::getPosition()
{
	Position pos;
	pos.x = get<uint16_t>();
	pos.y = get<uint16_t>();
	pos.z = getByte();
	return pos;
}

void NetworkMessage::addString(const std::string& value)
{
	size_t stringLen = value.length();
	if (!canAdd(stringLen + 2)) {
		return;
	}

	add<uint16_t>(stringLen);
	memcpy(buffer + info.position, value.c_str(), stringLen);
	info.position += stringLen;
	info.length += stringLen;
}

void NetworkMessage::addDouble(double value, uint8_t precision/* = 2*/)
{
	addByte(precision);
	add<uint32_t>((value * std::pow(static_cast<float>(10), precision)) + std::numeric_limits<int32_t>::max());
}

void NetworkMessage::addBytes(const char* bytes, size_t size)
{
	if (!canAdd(size)) {
		return;
	}

	memcpy(buffer + info.position, bytes, size);
	info.position += size;
	info.length += size;
}

void NetworkMessage::addPaddingBytes(size_t n)
{
	#define canAdd(size) ((size + info.position) < NETWORKMESSAGE_MAXSIZE)
	if (!canAdd(n)) {
		return;
	}
	#undef canAdd

	memset(buffer + info.position, 0x33, n);
	info.length += n;
}

void NetworkMessage::addPosition(const Position& pos)
{
	add<uint16_t>(pos.x);
	add<uint16_t>(pos.y);
	addByte(pos.z);
}

void NetworkMessage::addItemId(uint16_t itemId)
{
	add<uint16_t>(Item::items[itemId].clientId);
}

void NetworkMessage::addEffect(Effect effect) {
	add<uint16_t>(effect.getId());

	Pieces pieces = effect.getPieces();
	addByte(pieces.size()); // for now it's always 4
	for (auto const& effectPiece : pieces) {
		add<uint16_t>(effectPiece.id);
		addByte(effectPiece.childrenIds.size());

		for (auto const& id: effectPiece.childrenIds) {
			add<uint16_t>(id);
		}

		addPosition(effectPiece.offset);
	}
}

Effect NetworkMessage::getEffect()
{	
	Effect effect;

	effect.setId(get<uint16_t>()); // effect type
	uint8_t effectSize = getByte();
	for (uint8_t i = 0; i < effectSize; i++) {
		EffectPiece effectPiece;

		effectPiece.id = get<uint16_t>();
		uint8_t childrenSize = getByte();
		for (uint8_t j = 0; j < childrenSize; j++) {
			effectPiece.childrenIds.emplace_back(get<uint16_t>());
		}
		effectPiece.offset = getPosition();

		effect.addPiece(effectPiece);
	}
	return effect;
}