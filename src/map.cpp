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

#include "iomap.h"
#include "iomapserialize.h"
#include "combat.h"
#include "creature.h"
#include "monster.h"
#include "game.h"

extern Game g_game;

bool Map::loadMap(const std::string& identifier, bool loadHouses)
{
	IOMap loader;
	if (!loader.loadMap(this, identifier)) {
		std::cout << "[Fatal - Map::loadMap] " << loader.getLastErrorString() << std::endl;
		return false;
	}

	if (!IOMap::loadSpawns(this)) {
		std::cout << "[Warning - Map::loadMap] Failed to load spawn data." << std::endl;
	}

	if (loadHouses) {
		if (!IOMap::loadHouses(this)) {
			std::cout << "[Warning - Map::loadMap] Failed to load house data." << std::endl;
		}

		IOMapSerialize::loadHouseInfo();
		IOMapSerialize::loadHouseItems(this);
	}
	return true;
}

bool Map::save()
{
	bool saved = false;
	for (uint32_t tries = 0; tries < 3; tries++) {
		if (IOMapSerialize::saveHouseInfo()) {
			saved = true;
			break;
		}
	}

	if (!saved) {
		return false;
	}

	saved = false;
	for (uint32_t tries = 0; tries < 3; tries++) {
		if (IOMapSerialize::saveHouseItems()) {
			saved = true;
			break;
		}
	}
	return saved;
}

MapSector* Map::createMapSector(uint32_t x, uint32_t y)
{
	uint32_t index = (x / SECTOR_SIZE) | ((y / SECTOR_SIZE) << 16);
	auto it = mapSectors.find(index);
	if (it != mapSectors.end()) {
		return &it->second;
	}

	MapSector::newSector = true;
	return &mapSectors[index];
}

MapSector* Map::getMapSector(uint32_t x, uint32_t y)
{
	auto it = mapSectors.find((x / SECTOR_SIZE) | ((y / SECTOR_SIZE) << 16));
	if (it != mapSectors.end()) {
		return &it->second;
	}
	return nullptr;
}

const MapSector* Map::getMapSector(uint32_t x, uint32_t y) const
{
	auto it = mapSectors.find((x / SECTOR_SIZE) | ((y / SECTOR_SIZE) << 16));
	if (it != mapSectors.end()) {
		return &it->second;
	}
	return nullptr;
}

Tile* Map::getTile(uint16_t x, uint16_t y, uint8_t z) const
{
	if (z >= MAP_MAX_LAYERS) {
		return nullptr;
	}

	const MapSector* sector = getMapSector(x, y);
	if (!sector) {
		return nullptr;
	}

	return sector->tiles[z][x & SECTOR_MASK][y & SECTOR_MASK];
}

void Map::setTile(uint16_t x, uint16_t y, uint8_t z, Tile* newTile)
{
	if (z >= MAP_MAX_LAYERS) {
		std::cout << "ERROR: Attempt to set tile on invalid coordinate " << Position(x, y, z) << "!" << std::endl;
		return;
	}

	MapSector::newSector = false;
	MapSector* sector = createMapSector(x, y);

	if (MapSector::newSector) {
		//update north sector
		MapSector* northSector = getMapSector(x, y - SECTOR_SIZE);
		if (northSector) {
			northSector->sectorS = sector;
		}

		//update west sector
		MapSector* westSector = getMapSector(x - SECTOR_SIZE, y);
		if (westSector) {
			westSector->sectorE = sector;
		}

		//update south sector
		MapSector* southSector = getMapSector(x, y + SECTOR_SIZE);
		if (southSector) {
			sector->sectorS = southSector;
		}

		//update east sector
		MapSector* eastSector = getMapSector(x + SECTOR_SIZE, y);
		if (eastSector) {
			sector->sectorE = eastSector;
		}
	}

	sector->createFloor(z);
	Tile*& tile = sector->tiles[z][x & SECTOR_MASK][y & SECTOR_MASK];
	if (tile) {
		TileItemVector* items = newTile->getItemList();
		if (items) {
			for (auto it = items->begin(), end = items->end(); it != end; ++it) {
				tile->addThing(*it);
			}
			items->clear();
		}

		Item* ground = newTile->getGround();
		if (ground) {
			tile->addThing(ground);
			newTile->setGround(nullptr);
		}
		delete newTile;
	} else {
		tile = newTile;
	}
}

bool Map::placeCreature(const Position& centerPos, Creature* creature, bool extendedPos/* = false*/, bool forceLogin/* = false*/)
{
	bool foundTile;
	bool placeInPZ;

	Tile* tile = getTile(centerPos.x, centerPos.y, centerPos.z);
	if (tile) {
		placeInPZ = tile->hasFlag(TILESTATE_PROTECTIONZONE);
		ReturnValue ret = tile->queryAdd(0, *creature, 1, FLAG_IGNOREBLOCKITEM);
		foundTile = forceLogin || ret == RETURNVALUE_NOERROR || ret == RETURNVALUE_PLAYERISNOTINVITED;
	} else {
		placeInPZ = false;
		foundTile = false;
	}

	if (!foundTile) {
		static std::vector<std::pair<int32_t, int32_t>> extendedRelList {
			                   {0, -2},
			         {-1, -1}, {0, -1}, {1, -1},
			{-2, 0}, {-1,  0},          {1,  0}, {2, 0},
			         {-1,  1}, {0,  1}, {1,  1},
			                   {0,  2}
		};

		static std::vector<std::pair<int32_t, int32_t>> normalRelList {
			{-1, -1}, {0, -1}, {1, -1},
			{-1,  0},          {1,  0},
			{-1,  1}, {0,  1}, {1,  1}
		};

		std::vector<std::pair<int32_t, int32_t>>& relList = (extendedPos ? extendedRelList : normalRelList);

		if (extendedPos) {
			std::shuffle(relList.begin(), relList.begin() + 4, getRandomGenerator());
			std::shuffle(relList.begin() + 4, relList.end(), getRandomGenerator());
		} else {
			std::shuffle(relList.begin(), relList.end(), getRandomGenerator());
		}

		for (const auto& it : relList) {
			Position tryPos(centerPos.x + it.first, centerPos.y + it.second, centerPos.z);

			tile = getTile(tryPos.x, tryPos.y, tryPos.z);
			if (!tile || (placeInPZ && !tile->hasFlag(TILESTATE_PROTECTIONZONE))) {
				continue;
			}

			if (tile->queryAdd(0, *creature, 1, 0) == RETURNVALUE_NOERROR) {
				if (!extendedPos || isSightClear(centerPos, tryPos, false)) {
					foundTile = true;
					break;
				}
			}
		}

		if (!foundTile) {
			return false;
		}
	}

	int32_t index = 0;
	uint32_t flags = 0;
	Item* toItem = nullptr;

	Cylinder* toCylinder = tile->queryDestination(index, *creature, &toItem, flags);
	toCylinder->internalAddThing(creature);

	const Position& dest = toCylinder->getPosition();
	getMapSector(dest.x, dest.y)->addCreature(creature);
	return true;
}

void Map::moveCreature(Creature& creature, Tile& newTile, bool forceTeleport/* = false*/)
{
	Tile& oldTile = *creature.getTile();

	const Position& oldPos = oldTile.getPosition();
	const Position& newPos = newTile.getPosition();

	bool teleport = forceTeleport || !newTile.getGround() || !Position::areInRange<1, 1, 0>(oldPos, newPos);

	SpectatorVector spectators;
	if (!teleport) {
		int32_t minRangeX = maxViewportX;
		int32_t maxRangeX = maxViewportX;
		int32_t minRangeY = maxViewportY;
		int32_t maxRangeY = maxViewportY;
		if (oldPos.y > newPos.y) {
			++minRangeY;
		} else if (oldPos.y < newPos.y) {
			++maxRangeY;
		}

		if (oldPos.x < newPos.x) {
			++maxRangeX;
		} else if (oldPos.x > newPos.x) {
			++minRangeX;
		}
		getSpectators(spectators, oldPos, true, false, minRangeX, maxRangeX, minRangeY, maxRangeY);
	} else {
		SpectatorVector newspectators;
		getSpectators(spectators, oldPos, true);
		getSpectators(newspectators, newPos, true);
		spectators.mergeSpectators(newspectators);
	}

	std::vector<int32_t> oldStackPosVector(spectators.size());
	size_t i = static_cast<size_t>(-1); //Start index at -1 to avoid copying it
	for (Creature* spectator : spectators) {
		if (Player* tmpPlayer = spectator->getPlayer()) {
			if (tmpPlayer->canSeeCreature(&creature)) {
				oldStackPosVector[++i] = oldTile.getClientIndexOfCreature(tmpPlayer, &creature);
			} else {
				oldStackPosVector[++i] = -1;
			}
		}
	}

	//remove the creature
	oldTile.removeThing(&creature, 0);

	MapSector* old_sector = getMapSector(oldPos.x, oldPos.y);
	MapSector* new_sector = getMapSector(newPos.x, newPos.y);

	// Switch the node ownership
	if (old_sector != new_sector) {
		old_sector->removeCreature(&creature);
		new_sector->addCreature(&creature);
	}

	//add the creature
	newTile.addThing(&creature);

	if (!teleport) {
		if (oldPos.y > newPos.y) {
			creature.setDirection(DIRECTION_NORTH);
		} else if (oldPos.y < newPos.y) {
			creature.setDirection(DIRECTION_SOUTH);
		}

		if (oldPos.x < newPos.x) {
			creature.setDirection(DIRECTION_EAST);
		} else if (oldPos.x > newPos.x) {
			creature.setDirection(DIRECTION_WEST);
		}
	}

	//send to client + event method
	i = static_cast<size_t>(-1); //Start index at -1 to avoid copying it
	for (Creature* spectator : spectators) {
		if (Player* tmpPlayer = spectator->getPlayer()) {
			//Use the correct stackpos
			int32_t stackpos = oldStackPosVector[++i];
			if (stackpos != -1) {
				// 0xFF is special stackpos that tells client to insert it as new object - exactly what we want
				tmpPlayer->sendCreatureMove(&creature, newPos, 0xFF, oldPos, stackpos, teleport);
			}
		}

		spectator->onCreatureMove(&creature, &newTile, newPos, &oldTile, oldPos, teleport);
	}

	oldTile.postRemoveNotification(&creature, &newTile, 0);
	newTile.postAddNotification(&creature, &oldTile, 0);
}

std::vector<Tile*> Map::getFloorTiles(int32_t x, int32_t y, int32_t width, int32_t height, int32_t z)
{
	std::vector<Tile*> tileVector(width*height, nullptr);

	int32_t x1 = std::min<int32_t>(0xFFFF, std::max<int32_t>(0, x));
	int32_t y1 = std::min<int32_t>(0xFFFF, std::max<int32_t>(0, y));
	int32_t x2 = std::min<int32_t>(0xFFFF, std::max<int32_t>(0, (x + width)));
	int32_t y2 = std::min<int32_t>(0xFFFF, std::max<int32_t>(0, (y + height)));

	int32_t startx1 = x1 - (x1 & SECTOR_MASK);
	int32_t starty1 = y1 - (y1 & SECTOR_MASK);
	int32_t endx2 = x2 - (x2 & SECTOR_MASK);
	int32_t endy2 = y2 - (y2 & SECTOR_MASK);

	const MapSector* startSector = getMapSector(startx1, starty1);
	const MapSector* sectorS = startSector;
	const MapSector* sectorE;
	for (int32_t ny = starty1; ny <= endy2; ny += SECTOR_SIZE) {
		sectorE = sectorS;
		for (int32_t nx = startx1; nx <= endx2; nx += SECTOR_SIZE) {
			if (sectorE) {
				if (sectorE->getFloor(z)) {
					int32_t tx = nx;
					for (auto& row : sectorE->tiles[z]) {
						if (static_cast<uint32_t>(tx - x) < static_cast<uint32_t>(width)) {
							int32_t ty = ny;
							uint32_t index = ((tx - x) * height) + (ty - y);
							for (auto tile : row) {
								if (static_cast<uint32_t>(ty - y) < static_cast<uint32_t>(height)) {
									tileVector[index] = tile;
								}
								++index;
								++ty;
							}
						}
						++tx;
					}
				}
				sectorE = sectorE->sectorE;
			} else {
				sectorE = getMapSector(nx + SECTOR_SIZE, ny);
			}
		}

		if (sectorS) {
			sectorS = sectorS->sectorS;
		} else {
			sectorS = getMapSector(startx1, ny + SECTOR_SIZE);
		}
	}
	return tileVector;
}

void Map::getSpectatorsInternal(SpectatorVector& spectators, const Position& centerPos, int32_t minRangeX, int32_t maxRangeX, int32_t minRangeY, int32_t maxRangeY, int32_t minRangeZ, int32_t maxRangeZ, bool onlyPlayers) const
{
	int32_t min_y = centerPos.y - minRangeY;
	int32_t min_x = centerPos.x - minRangeX;
	int32_t max_y = centerPos.y + maxRangeY;
	int32_t max_x = centerPos.x + maxRangeX;

	uint32_t width = static_cast<uint32_t>(max_x - min_x);
	uint32_t height = static_cast<uint32_t>(max_y - min_y);
	uint32_t depth = static_cast<uint32_t>(maxRangeZ - minRangeZ);

	int32_t minoffset = centerPos.getZ() - maxRangeZ;
	int32_t x1 = std::min<int32_t>(0xFFFF, std::max<int32_t>(0, (min_x + minoffset)));
	int32_t y1 = std::min<int32_t>(0xFFFF, std::max<int32_t>(0, (min_y + minoffset)));

	int32_t maxoffset = centerPos.getZ() - minRangeZ;
	int32_t x2 = std::min<int32_t>(0xFFFF, std::max<int32_t>(0, (max_x + maxoffset)));
	int32_t y2 = std::min<int32_t>(0xFFFF, std::max<int32_t>(0, (max_y + maxoffset)));

	int32_t startx1 = x1 - (x1 & SECTOR_MASK);
	int32_t starty1 = y1 - (y1 & SECTOR_MASK);
	int32_t endx2 = x2 - (x2 & SECTOR_MASK);
	int32_t endy2 = y2 - (y2 & SECTOR_MASK);

	const MapSector* startSector = getMapSector(startx1, starty1);
	const MapSector* sectorS = startSector;
	const MapSector* sectorE;
	for (int32_t ny = starty1; ny <= endy2; ny += SECTOR_SIZE) {
		sectorE = sectorS;
		for (int32_t nx = startx1; nx <= endx2; nx += SECTOR_SIZE) {
			if (sectorE) {
				const CreatureVector& node_list = (onlyPlayers ? sectorE->player_list : sectorE->creature_list);
				for (auto it = node_list.begin(), end = node_list.end(); it != end; ++it) {
					Creature* creature = (*it);

					const Position& cpos = creature->getPosition();
					if (static_cast<uint32_t>(static_cast<int32_t>(cpos.z) - minRangeZ) <= depth) {
						int_fast16_t offsetZ = Position::getOffsetZ(centerPos, cpos);
						if (static_cast<uint32_t>(static_cast<int32_t>(cpos.x - offsetZ) - min_x) <= width && static_cast<uint32_t>(static_cast<int32_t>(cpos.y - offsetZ) - min_y) <= height) {
							spectators.push_back(creature);
						}
					}
				}
				sectorE = sectorE->sectorE;
			} else {
				sectorE = getMapSector(nx + SECTOR_SIZE, ny);
			}
		}

		if (sectorS) {
			sectorS = sectorS->sectorS;
		} else {
			sectorS = getMapSector(startx1, ny + SECTOR_SIZE);
		}
	}
}

void Map::getSpectators(SpectatorVector& spectators, const Position& centerPos, bool multifloor /*= false*/, bool onlyPlayers /*= false*/, int32_t minRangeX /*= 0*/, int32_t maxRangeX /*= 0*/, int32_t minRangeY /*= 0*/, int32_t maxRangeY /*= 0*/)
{
	if (centerPos.z >= MAP_MAX_LAYERS) {
		return;
	}

	bool foundCache = false;
	bool cacheResult = false;

	minRangeX = (minRangeX == 0 ? maxViewportX : minRangeX);
	maxRangeX = (maxRangeX == 0 ? maxViewportX : maxRangeX);
	minRangeY = (minRangeY == 0 ? maxViewportY : minRangeY);
	maxRangeY = (maxRangeY == 0 ? maxViewportY : maxRangeY);
	if (minRangeX == maxViewportX && maxRangeX == maxViewportX && minRangeY == maxViewportY && maxRangeY == maxViewportY && multifloor) {
		if (onlyPlayers) {
			auto it = playersSpectatorCache.find(centerPos);
			if (it != playersSpectatorCache.end()) {
				if (!spectators.empty()) {
					const SpectatorVector& cachedSpectators = it->second;
					spectators.insert(spectators.end(), cachedSpectators.begin(), cachedSpectators.end());
				} else {
					spectators = it->second;
				}

				foundCache = true;
			}
		}

		if (!foundCache) {
			auto it = spectatorCache.find(centerPos);
			if (it != spectatorCache.end()) {
				if (!onlyPlayers) {
					if (!spectators.empty()) {
						const SpectatorVector& cachedSpectators = it->second;
						spectators.insert(spectators.end(), cachedSpectators.begin(), cachedSpectators.end());
					} else {
						spectators = it->second;
					}
				} else {
					const SpectatorVector& cachedSpectators = it->second;
					for (Creature* spectator : cachedSpectators) {
						if (spectator->getPlayer()) {
							spectators.emplace_back(spectator);
						}
					}
				}

				foundCache = true;
			} else {
				cacheResult = true;
			}
		}
	}

	if (!foundCache) {
		int32_t minRangeZ;
		int32_t maxRangeZ;
		if (multifloor) {
			if (centerPos.z > 7) {
				//underground

				//8->15
				minRangeZ = std::max<int32_t>(centerPos.getZ() - 2, 0);
				maxRangeZ = std::min<int32_t>(centerPos.getZ() + 2, MAP_MAX_LAYERS - 1);
			} else if (centerPos.z == 6) {
				minRangeZ = 0;
				maxRangeZ = 8;
			} else if (centerPos.z == 7) {
				minRangeZ = 0;
				maxRangeZ = 9;
			} else {
				minRangeZ = 0;
				maxRangeZ = 7;
			}
		} else {
			minRangeZ = centerPos.z;
			maxRangeZ = centerPos.z;
		}
		if (spectators.capacity() < 32) {
			spectators.reserve(32);
		}

		getSpectatorsInternal(spectators, centerPos, minRangeX, maxRangeX, minRangeY, maxRangeY, minRangeZ, maxRangeZ, onlyPlayers);
		if (cacheResult) {
			if (onlyPlayers) {
				playersSpectatorCache[centerPos] = spectators;
			} else {
				spectatorCache[centerPos] = spectators;
			}
		}
	}
}

void Map::clearSpectatorCache(bool clearPlayer)
{
	spectatorCache.clear();
	if (clearPlayer) {
		playersSpectatorCache.clear();
	}
}

bool Map::canThrowObjectTo(const Position& fromPos, const Position& toPos, bool checkLineOfSight /*= true*/,
                           int32_t rangex /*= Map::maxClientViewportX*/, int32_t rangey /*= Map::maxClientViewportY*/) const
{
	//z checks
	//underground 8->15
	//ground level and above 7->0
	if ((fromPos.z >= 8 && toPos.z < 8) || (toPos.z >= 8 && fromPos.z < 8)) {
		return false;
	}

	int32_t deltaz = Position::getDistanceZ(fromPos, toPos);
	if (deltaz > 2) {
		return false;
	}

	if ((Position::getDistanceX(fromPos, toPos) - deltaz) > rangex) {
		return false;
	}

	//distance checks
	if ((Position::getDistanceY(fromPos, toPos) - deltaz) > rangey) {
		return false;
	}

	if (!checkLineOfSight) {
		return true;
	}
	return isSightClear(fromPos, toPos, false);
}

bool Map::checkSightLine(const Position& fromPos, const Position& toPos) const
{
	if (fromPos == toPos) {
		return true;
	}

	Position start(fromPos.z > toPos.z ? toPos : fromPos);
	Position destination(fromPos.z > toPos.z ? fromPos : toPos);

	int32_t distanceX = Position::getDistanceX(start, destination);
	int32_t distanceY = Position::getDistanceY(start, destination);

	if (start.y == destination.y) {
		// Horizontal line
		const uint16_t delta = (start.x < destination.x ? 0x0001 : 0xFFFF);
		while (--distanceX) {
			start.x += delta;

			const Tile* tile = getTile(start.x, start.y, start.z);
			if (tile && tile->hasFlag(TILESTATE_BLOCKPROJECTILE)) {
				return false;
			}
		}
	} else if (start.x == destination.x) {
		// Vertical line
		const uint16_t delta = (start.y < destination.y ? 0x0001 : 0xFFFF);
		while (--distanceY){
			start.y += delta;

			const Tile* tile = getTile(start.x, start.y, start.z);
			if (tile && tile->hasFlag(TILESTATE_BLOCKPROJECTILE)) {
				return false;
			}
		}
	} else if (distanceX == distanceY) {
		// Diagonal line
		const uint16_t deltaX = (start.x < destination.x ? 0x0001 : 0xFFFF);
		const uint16_t deltaY = (start.y < destination.y ? 0x0001 : 0xFFFF);
		while (--distanceX) {
			start.x += deltaX;
			start.y += deltaY;

			const Tile* tile = getTile(start.x, start.y, start.z);
			if (tile && tile->hasFlag(TILESTATE_BLOCKPROJECTILE)) {
				return false;
			}
		}
	} else {
		const uint16_t deltaX = (start.x < destination.x ? 0x0001 : 0xFFFF);
		const uint16_t deltaY = (start.y < destination.y ? 0x0001 : 0xFFFF);

		#if GAME_FEATURE_XIAOLIN_WU_SIGHT_CLEAR > 0
		// Xiaolin Wu's line algorithm
		uint16_t eAdj, eAcc = 0;

		if (distanceY > distanceX) {
			eAdj = (static_cast<uint32_t>(distanceX) << 16) / static_cast<uint32_t>(distanceY);

			while (--distanceY) {
				uint16_t xIncrease = 0, eAccTemp = eAcc;
				eAcc += eAdj;
				if (eAcc <= eAccTemp) {
					xIncrease = deltaX;
				}

				const Tile* tile = getTile(start.x + xIncrease, start.y + deltaY, start.z);
				if (tile && tile->hasFlag(TILESTATE_BLOCKPROJECTILE)) {
					if (Position::areInRange<1, 1>(start, destination)) {
						break;
					} else {
						return false;
					}
				}

				start.x += xIncrease;
				start.y += deltaY;
			}
		} else {
			eAdj = (static_cast<uint32_t>(distanceY) << 16) / static_cast<uint32_t>(distanceX);

			while (--distanceX) {
				uint16_t yIncrease = 0, eAccTemp = eAcc;
				eAcc += eAdj;
				if (eAcc <= eAccTemp) {
					yIncrease = deltaY;
				}

				const Tile* tile = getTile(start.x + deltaX, start.y + yIncrease, start.z);
				if (tile && tile->hasFlag(TILESTATE_BLOCKPROJECTILE)) {
					if (Position::areInRange<1, 1>(start, destination)) {
						break;
					} else {
						return false;
					}
				}

				start.x += deltaX;
				start.y += yIncrease;
			}
		}
		#else
		// Bresenham's line algorithm
		int32_t distance, delta, d1Increase, d2Increase;
		uint16_t x1Increase, x2Increase;
		uint16_t y1Increase, y2Increase;

		if (distanceX >= distanceY) {
			distance = distanceX;
			delta = (2 * distanceY) - distanceX;
			d1Increase = distanceY * 2;
			d2Increase = (distanceY - distanceX) * 2;
			x1Increase = deltaX;
			x2Increase = deltaX;
			y1Increase = 0;
			y2Increase = deltaY;
		} else {
			distance = distanceY;
			delta = (2 * distanceX) - distanceY;
			d1Increase = distanceX * 2;
			d2Increase = (distanceX - distanceY) * 2;
			x1Increase = 0;
			x2Increase = deltaX;
			y1Increase = deltaY;
			y2Increase = deltaY;
		}

		while (--distance) {
			uint16_t xIncrease = 0, yIncrease = 0;
			if (delta < 0) {
				delta += d1Increase;
				xIncrease = x1Increase;
				yIncrease = y1Increase;
			} else {
				delta += d2Increase;
				xIncrease = x2Increase;
				yIncrease = y2Increase;
			}

			const Tile* tile = getTile(start.x + xIncrease, start.y + yIncrease, start.z);
			if (tile && tile->hasFlag(TILESTATE_BLOCKPROJECTILE)) {
				if (Position::areInRange<1, 1>(start, destination)) {
					break;
				} else {
					return false;
				}
			}

			start.x += xIncrease;
			start.y += yIncrease;
		}
		#endif
	}
	
	// now we need to perform a jump between floors to see if everything is clear (literally)
	while (start.z != destination.z) {
		// use destination x and y because we might end up with different start x and y values
		const Tile* tile = getTile(destination.x, destination.y, start.z);
		if (tile && tile->getThingCount() > 0) {
			return false;
		}

		start.z++;
	}
	
	return true;
}

bool Map::isSightClear(const Position& fromPos, const Position& toPos, bool floorCheck) const
{
	if (floorCheck && fromPos.z != toPos.z) {
		return false;
	}

	// Perform checking destination first
	const Tile* tile = getTile(toPos.x, toPos.y, (fromPos.z > toPos.z ? toPos.z : fromPos.z));
	if (tile && tile->hasFlag(TILESTATE_BLOCKPROJECTILE)) {
		return false;
	} else {
		// Check if we even need to perform line checking
		if (fromPos.z == toPos.z && Position::areInRange<1, 1>(fromPos, toPos)) {
			return true;
		}
	}

	// Cast two converging rays and see if either yields a result.
	return checkSightLine(fromPos, toPos) || checkSightLine(toPos, fromPos);
}

const Tile* Map::canWalkTo(const Creature& creature, const Position& pos) const
{
	int32_t walkCache = creature.getWalkCache(pos);
	if (walkCache == 0) {
		return nullptr;
	} else if (walkCache == 1) {
		return getTile(pos.x, pos.y, pos.z);
	}

	//used for non-cached tiles
	Tile* tile = getTile(pos.x, pos.y, pos.z);
	if (!tile || tile->queryAdd(0, creature, 1, FLAG_PATHFINDING | FLAG_IGNOREFIELDDAMAGE) != RETURNVALUE_NOERROR) {
		return nullptr;
	}
	return tile;
}

bool Map::getPathMatching(const Creature& creature, const Position& targetPos, std::vector<Direction>& dirList, const FrozenPathingConditionCall& pathCondition, const FindPathParams& fpp) const
{
	Position pos = creature.getPosition();
	Position endPos;

	AStarNodes nodes(pos.x, pos.y, AStarNodes::getTileWalkCost(creature, getTile(pos.x, pos.y, pos.z)));

	int32_t bestMatch = 0;

	static int_fast32_t dirNeighbors[8][5][2] = {
		{{-1, 0}, {0, 1}, {1, 0}, {1, 1}, {-1, 1}},
		{{-1, 0}, {0, 1}, {0, -1}, {-1, -1}, {-1, 1}},
		{{-1, 0}, {1, 0}, {0, -1}, {-1, -1}, {1, -1}},
		{{0, 1}, {1, 0}, {0, -1}, {1, -1}, {1, 1}},
		{{1, 0}, {0, -1}, {-1, -1}, {1, -1}, {1, 1}},
		{{-1, 0}, {0, -1}, {-1, -1}, {1, -1}, {-1, 1}},
		{{0, 1}, {1, 0}, {1, -1}, {1, 1}, {-1, 1}},
		{{-1, 0}, {0, 1}, {-1, -1}, {1, 1}, {-1, 1}}
	};
	static int_fast32_t allNeighbors[8][2] = {
		{-1, 0}, {0, 1}, {1, 0}, {0, -1}, {-1, -1}, {1, -1}, {1, 1}, {-1, 1}
	};

	const Position startPos = pos;

	const int_fast32_t sX = std::abs(targetPos.getX() - pos.getX());
	const int_fast32_t sY = std::abs(targetPos.getY() - pos.getY());

	AStarNode* found = nullptr;
	while (fpp.maxSearchDist != 0 || nodes.getClosedNodes() < 100) {
		AStarNode* n = nodes.getBestNode();
		if (!n) {
			if (found) {
				break;
			}
			return false;
		}

		const int_fast32_t x = n->x;
		const int_fast32_t y = n->y;
		pos.x = x;
		pos.y = y;
		if (pathCondition(startPos, pos, fpp, bestMatch)) {
			found = n;
			endPos = pos;
			if (bestMatch == 0) {
				break;
			}
		}

		uint_fast32_t dirCount;
		int_fast32_t* neighbors;
		if (n->parent) {
			const int_fast32_t offset_x = n->parent->x - x;
			const int_fast32_t offset_y = n->parent->y - y;
			if (offset_y == 0) {
				if (offset_x == -1) {
					neighbors = *dirNeighbors[DIRECTION_WEST];
				} else {
					neighbors = *dirNeighbors[DIRECTION_EAST];
				}
			} else if (offset_x == 0) {
				if (offset_y == -1) {
					neighbors = *dirNeighbors[DIRECTION_NORTH];
				} else {
					neighbors = *dirNeighbors[DIRECTION_SOUTH];
				}
			} else if (offset_y == -1) {
				if (offset_x == -1) {
					neighbors = *dirNeighbors[DIRECTION_NORTHWEST];
				} else {
					neighbors = *dirNeighbors[DIRECTION_NORTHEAST];
				}
			} else if (offset_x == -1) {
				neighbors = *dirNeighbors[DIRECTION_SOUTHWEST];
			} else {
				neighbors = *dirNeighbors[DIRECTION_SOUTHEAST];
			}
			dirCount = 5;
		} else {
			dirCount = 8;
			neighbors = *allNeighbors;
		}

		const int_fast32_t f = n->f;
		for (uint_fast32_t i = 0; i < dirCount; ++i) {
			pos.x = x + *neighbors++;
			pos.y = y + *neighbors++;

			const Tile* tile;
			int_fast32_t extraCost;
			AStarNode* neighborNode = nodes.getNodeByPosition(pos.x, pos.y);
			if (neighborNode) {
				tile = getTile(pos.x, pos.y, pos.z);
				extraCost = neighborNode->c;
			} else {
				tile = Map::canWalkTo(creature, pos);
				if (!tile) {
					continue;
				}
				extraCost = AStarNodes::getTileWalkCost(creature, tile);
			}

			//The cost (g) for this neighbor
			const int_fast32_t cost = AStarNodes::getMapWalkCost(n, pos);
			const int_fast32_t newf = f + cost + extraCost;
			if (neighborNode) {
				if (neighborNode->f <= newf) {
					//The node on the closed/open list is cheaper than this one
					continue;
				}
				neighborNode->f = newf;
				neighborNode->parent = n;
				nodes.openNode(neighborNode);
			} else {
				//Does not exist in the open/closed list, create a new node
				const int_fast32_t dX = std::abs(targetPos.getX() - pos.getX());
				const int_fast32_t dY = std::abs(targetPos.getY() - pos.getY());
				if (!nodes.createOpenNode(n, pos.x, pos.y, newf, ((dX - sX) << 3) + ((dY - sY) << 3) + (std::max(dX, dY) << 3), extraCost)) {
					if (found) {
						break;
					}
					return false;
				}
			}
		}
		nodes.closeNode(n);
	}
	if (!found) {
		return false;
	}

	int_fast32_t prevx = endPos.x;
	int_fast32_t prevy = endPos.y;

	found = found->parent;
	while (found) {
		pos.x = found->x;
		pos.y = found->y;

		int_fast32_t dx = pos.getX() - prevx;
		int_fast32_t dy = pos.getY() - prevy;

		prevx = pos.x;
		prevy = pos.y;
		if (dx == 1) {
			if (dy == 1) {
				dirList.emplace_back(DIRECTION_NORTHWEST);
			} else if (dy == -1) {
				dirList.emplace_back(DIRECTION_SOUTHWEST);
			} else {
				dirList.emplace_back(DIRECTION_WEST);
			}
		} else if (dx == -1) {
			if (dy == 1) {
				dirList.emplace_back(DIRECTION_NORTHEAST);
			} else if (dy == -1) {
				dirList.emplace_back(DIRECTION_SOUTHEAST);
			} else {
				dirList.emplace_back(DIRECTION_EAST);
			}
		} else if (dy == 1) {
			dirList.emplace_back(DIRECTION_NORTH);
		} else if (dy == -1) {
			dirList.emplace_back(DIRECTION_SOUTH);
		}
		found = found->parent;
	}
	return true;
}

bool Map::getPathMatchingCond(const Creature& creature, const Position& targetPos, std::vector<Direction>& dirList, const FrozenPathingConditionCall& pathCondition, const FindPathParams& fpp) const
{
	Position pos = creature.getPosition();
	Position endPos;

	AStarNodes nodes(pos.x, pos.y, AStarNodes::getTileWalkCost(creature, getTile(pos.x, pos.y, pos.z)));

	int32_t bestMatch = 0;

	static int_fast32_t dirNeighbors[8][5][2] = {
		{{-1, 0}, {0, 1}, {1, 0}, {1, 1}, {-1, 1}},
		{{-1, 0}, {0, 1}, {0, -1}, {-1, -1}, {-1, 1}},
		{{-1, 0}, {1, 0}, {0, -1}, {-1, -1}, {1, -1}},
		{{0, 1}, {1, 0}, {0, -1}, {1, -1}, {1, 1}},
		{{1, 0}, {0, -1}, {-1, -1}, {1, -1}, {1, 1}},
		{{-1, 0}, {0, -1}, {-1, -1}, {1, -1}, {-1, 1}},
		{{0, 1}, {1, 0}, {1, -1}, {1, 1}, {-1, 1}},
		{{-1, 0}, {0, 1}, {-1, -1}, {1, 1}, {-1, 1}}
	};
	static int_fast32_t allNeighbors[8][2] = {
		{-1, 0}, {0, 1}, {1, 0}, {0, -1}, {-1, -1}, {1, -1}, {1, 1}, {-1, 1}
	};

	const Position startPos = pos;

	const int_fast32_t sX = std::abs(targetPos.getX() - pos.getX());
	const int_fast32_t sY = std::abs(targetPos.getY() - pos.getY());

	AStarNode* found = nullptr;
	while (fpp.maxSearchDist != 0 || nodes.getClosedNodes() < 100) {
		AStarNode* n = nodes.getBestNode();
		if (!n) {
			if (found) {
				break;
			}
			return false;
		}

		const int_fast32_t x = n->x;
		const int_fast32_t y = n->y;
		pos.x = x;
		pos.y = y;
		if (pathCondition(startPos, pos, fpp, bestMatch)) {
			found = n;
			endPos = pos;
			if (bestMatch == 0) {
				break;
			}
		}

		uint_fast32_t dirCount;
		int_fast32_t* neighbors;
		if (n->parent) {
			const int_fast32_t offset_x = n->parent->x - x;
			const int_fast32_t offset_y = n->parent->y - y;
			if (offset_y == 0) {
				if (offset_x == -1) {
					neighbors = *dirNeighbors[DIRECTION_WEST];
				} else {
					neighbors = *dirNeighbors[DIRECTION_EAST];
				}
			} else if (offset_x == 0) {
				if (offset_y == -1) {
					neighbors = *dirNeighbors[DIRECTION_NORTH];
				} else {
					neighbors = *dirNeighbors[DIRECTION_SOUTH];
				}
			} else if (offset_y == -1) {
				if (offset_x == -1) {
					neighbors = *dirNeighbors[DIRECTION_NORTHWEST];
				} else {
					neighbors = *dirNeighbors[DIRECTION_NORTHEAST];
				}
			} else if (offset_x == -1) {
				neighbors = *dirNeighbors[DIRECTION_SOUTHWEST];
			} else {
				neighbors = *dirNeighbors[DIRECTION_SOUTHEAST];
			}
			dirCount = 5;
		} else {
			dirCount = 8;
			neighbors = *allNeighbors;
		}

		const int_fast32_t f = n->f;
		for (uint_fast32_t i = 0; i < dirCount; ++i) {
			pos.x = x + *neighbors++;
			pos.y = y + *neighbors++;
			if (fpp.maxSearchDist != 0 && (Position::getDistanceX(startPos, pos) > fpp.maxSearchDist || Position::getDistanceY(startPos, pos) > fpp.maxSearchDist)) {
				continue;
			}

			if (fpp.keepDistance && !pathCondition.isInRange(startPos, pos, fpp)) {
				continue;
			}

			const Tile* tile;
			int_fast32_t extraCost;
			AStarNode* neighborNode = nodes.getNodeByPosition(pos.x, pos.y);
			if (neighborNode) {
				tile = getTile(pos.x, pos.y, pos.z);
				extraCost = neighborNode->c;
			} else {
				tile = Map::canWalkTo(creature, pos);
				if (!tile) {
					continue;
				}
				extraCost = AStarNodes::getTileWalkCost(creature, tile);
			}

			//The cost (g) for this neighbor
			const int_fast32_t cost = AStarNodes::getMapWalkCost(n, pos);
			const int_fast32_t newf = f + cost + extraCost;
			if (neighborNode) {
				if (neighborNode->f <= newf) {
					//The node on the closed/open list is cheaper than this one
					continue;
				}
				neighborNode->f = newf;
				neighborNode->parent = n;
				nodes.openNode(neighborNode);
			} else {
				//Does not exist in the open/closed list, create a new node
				const int_fast32_t dX = std::abs(targetPos.getX() - pos.getX());
				const int_fast32_t dY = std::abs(targetPos.getY() - pos.getY());
				if (!nodes.createOpenNode(n, pos.x, pos.y, newf, ((dX - sX) << 3) + ((dY - sY) << 3) + (std::max(dX, dY) << 3), extraCost)) {
					if (found) {
						break;
					}
					return false;
				}
			}
		}
		nodes.closeNode(n);
	}

	if (!found) {
		return false;
	}

	int_fast32_t prevx = endPos.x;
	int_fast32_t prevy = endPos.y;

	found = found->parent;
	while (found) {
		pos.x = found->x;
		pos.y = found->y;

		int_fast32_t dx = pos.getX() - prevx;
		int_fast32_t dy = pos.getY() - prevy;

		prevx = pos.x;
		prevy = pos.y;
		if (dx == 1) {
			if (dy == 1) {
				dirList.emplace_back(DIRECTION_NORTHWEST);
			} else if (dy == -1) {
				dirList.emplace_back(DIRECTION_SOUTHWEST);
			} else {
				dirList.emplace_back(DIRECTION_WEST);
			}
		} else if (dx == -1) {
			if (dy == 1) {
				dirList.emplace_back(DIRECTION_NORTHEAST);
			} else if (dy == -1) {
				dirList.emplace_back(DIRECTION_SOUTHEAST);
			} else {
				dirList.emplace_back(DIRECTION_EAST);
			}
		} else if (dy == 1) {
			dirList.emplace_back(DIRECTION_NORTH);
		} else if (dy == -1) {
			dirList.emplace_back(DIRECTION_SOUTH);
		}
		found = found->parent;
	}
	return true;
}

// AStarNodes
AStarNodes::AStarNodes(uint32_t x, uint32_t y, int_fast32_t extraCost): nodes(), openNodes()
{
	#if defined(__SSE2__)
	uint32_t defaultCost = std::numeric_limits<int32_t>::max();
	for (int32_t i = 0; i < 512; ++i) {
		memcpy(&calculatedNodes[i], &defaultCost, sizeof(calculatedNodes[0]));
	}
	#endif

	curNode = 1;
	closedNodes = 0;
	openNodes[0] = true;

	AStarNode& startNode = nodes[0];
	startNode.parent = nullptr;
	startNode.x = x;
	startNode.y = y;
	startNode.f = 0;
	startNode.g = 0;
	startNode.c = extraCost;
	nodesTable[0] = (x << 16) | y;
	#if defined(__SSE2__)
	calculatedNodes[0] = 0;
	#endif
}

bool AStarNodes::createOpenNode(AStarNode* parent, uint32_t x, uint32_t y, int_fast32_t f, int_fast32_t heuristic, int_fast32_t extraCost)
{
	if (curNode >= MAX_NODES) {
		return false;
	}

	int32_t retNode = curNode++;
	openNodes[retNode] = true;

	AStarNode& node = nodes[retNode];
	node.parent = parent;
	node.x = x;
	node.y = y;
	node.f = f;
	node.g = heuristic;
	node.c = extraCost;
	nodesTable[retNode] = (x << 16) | y;
	#if defined(__SSE2__)
	calculatedNodes[retNode] = f + node.g;
	#endif
	return true;
}

AStarNode* AStarNodes::getBestNode()
{
	//Branchless best node search
	#if defined(__AVX512F__)
	const __m512i increment = _mm512_set1_epi32(16);
	__m512i indices = _mm512_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
	__m512i minindices = indices;
	__m512i minvalues = _mm512_load_si512(reinterpret_cast<const void*>(calculatedNodes));
	for (int32_t pos = 16; pos < curNode; pos += 16) {
		const __m512i values = _mm512_load_si512(reinterpret_cast<const void*>(&calculatedNodes[pos]));
		indices = _mm512_add_epi32(indices, increment);
		minindices = _mm512_mask_blend_epi32(_mm512_cmplt_epi32_mask(values, minvalues), minindices, indices);
		minvalues = _mm512_min_epi32(minvalues, values);
	}

	alignas(64) int32_t values_array[16];
	alignas(64) int32_t indices_array[16];
	_mm512_store_si512(reinterpret_cast<void*>(values_array), minvalues);
	_mm512_store_si512(reinterpret_cast<void*>(indices_array), minindices);

	int32_t best_node = indices_array[0];
	int32_t best_node_f = values_array[0];
	for (int32_t i = 1; i < 16; ++i) {
		int32_t total_cost = values_array[i];
		best_node = (total_cost < best_node_f ? indices_array[i] : best_node);
		best_node_f = (total_cost < best_node_f ? total_cost : best_node_f);
	}
	return (openNodes[best_node] ? &nodes[best_node] : NULL);
	#elif defined(__AVX2__)
	const __m256i increment = _mm256_set1_epi32(8);
	__m256i indices = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
	__m256i minindices = indices;
	__m256i minvalues = _mm256_load_si256(reinterpret_cast<const __m256i*>(calculatedNodes));
	for (int32_t pos = 8; pos < curNode; pos += 8) {
		const __m256i values = _mm256_load_si256(reinterpret_cast<const __m256i*>(&calculatedNodes[pos]));
		indices = _mm256_add_epi32(indices, increment);
		minindices = _mm256_blendv_epi8(minindices, indices, _mm256_cmpgt_epi32(minvalues, values));
		minvalues = _mm256_min_epi32(values, minvalues);
	}

	alignas(32) int32_t values_array[8];
	alignas(32) int32_t indices_array[8];
	_mm256_store_si256(reinterpret_cast<__m256i*>(values_array), minvalues);
	_mm256_store_si256(reinterpret_cast<__m256i*>(indices_array), minindices);

	int32_t best_node = indices_array[0];
	int32_t best_node_f = values_array[0];
	for (int32_t i = 1; i < 8; ++i) {
		int32_t total_cost = values_array[i];
		best_node = (total_cost < best_node_f ? indices_array[i] : best_node);
		best_node_f = (total_cost < best_node_f ? total_cost : best_node_f);
	}
	return (openNodes[best_node] ? &nodes[best_node] : NULL);
	#elif defined(__SSE4_1__)
	const __m128i increment = _mm_set1_epi32(4);
	__m128i indices = _mm_setr_epi32(0, 1, 2, 3);
	__m128i minindices = indices;
	__m128i minvalues = _mm_load_si128(reinterpret_cast<const __m128i*>(calculatedNodes));
	for (int32_t pos = 4; pos < curNode; pos += 4) {
		const __m128i values = _mm_load_si128(reinterpret_cast<const __m128i*>(&calculatedNodes[pos]));
		indices = _mm_add_epi32(indices, increment);
		minindices = _mm_blendv_epi8(minindices, indices, _mm_cmplt_epi32(values, minvalues));
		minvalues = _mm_min_epi32(values, minvalues);
	}

	alignas(16) int32_t values_array[4];
	alignas(16) int32_t indices_array[4];
	_mm_store_si128(reinterpret_cast<__m128i*>(values_array), minvalues);
	_mm_store_si128(reinterpret_cast<__m128i*>(indices_array), minindices);

	int32_t best_node = indices_array[0];
	int32_t best_node_f = values_array[0];
	for (int32_t i = 1; i < 4; ++i) {
		int32_t total_cost = values_array[i];
		best_node = (total_cost < best_node_f ? indices_array[i] : best_node);
		best_node_f = (total_cost < best_node_f ? total_cost : best_node_f);
	}
	return (openNodes[best_node] ? &nodes[best_node] : NULL);
	#elif defined(__SSE2__)
	auto _mm_sse2_min_epi32 = [](const __m128i a, const __m128i b) {
		__m128i mask = _mm_cmpgt_epi32(a, b);
		return _mm_or_si128(_mm_and_si128(mask, b), _mm_andnot_si128(mask, a));
	};

	auto _mm_sse2_blendv_epi8 = [](const __m128i a, const __m128i b, __m128i mask) {
		mask = _mm_cmplt_epi8(mask, _mm_setzero_si128());
		return _mm_or_si128(_mm_andnot_si128(mask, a), _mm_and_si128(mask, b));
	};

	const __m128i increment = _mm_set1_epi32(4);
	__m128i indices = _mm_setr_epi32(0, 1, 2, 3);
	__m128i minindices = indices;
	__m128i minvalues = _mm_load_si128(reinterpret_cast<const __m128i*>(calculatedNodes));
	for (int32_t pos = 4; pos < curNode; pos += 4) {
		const __m128i values = _mm_load_si128(reinterpret_cast<const __m128i*>(&calculatedNodes[pos]));
		indices = _mm_add_epi32(indices, increment);
		minindices = _mm_sse2_blendv_epi8(minindices, indices, _mm_cmplt_epi32(values, minvalues));
		minvalues = _mm_sse2_min_epi32(values, minvalues);
	}

	alignas(16) int32_t values_array[4];
	alignas(16) int32_t indices_array[4];
	_mm_store_si128(reinterpret_cast<__m128i*>(values_array), minvalues);
	_mm_store_si128(reinterpret_cast<__m128i*>(indices_array), minindices);

	int32_t best_node = indices_array[0];
	int32_t best_node_f = values_array[0];
	for (int32_t i = 1; i < 4; ++i) {
		int32_t total_cost = values_array[i];
		best_node = (total_cost < best_node_f ? indices_array[i] : best_node);
		best_node_f = (total_cost < best_node_f ? total_cost : best_node_f);
	}
	return (openNodes[best_node] ? &nodes[best_node] : NULL);
	#else
	int32_t best_node_f = std::numeric_limits<int32_t>::max();
	int32_t best_node = -1;
	for (int32_t pos = 0; pos < curNode; ++pos) {
		if (!openNodes[pos]) {
			continue;
		}

		int32_t total_cost = nodes[pos].f + nodes[pos].g;
		best_node = (total_cost < best_node_f ? pos : best_node);
		best_node_f = (total_cost < best_node_f ? total_cost : best_node_f);
	}
	return (best_node != -1 ? &nodes[best_node] : nullptr);
	#endif
}

void AStarNodes::closeNode(AStarNode* node)
{
	size_t index = node - nodes;
	assert(index < MAX_NODES);
	#if defined(__SSE2__)
	calculatedNodes[index] = std::numeric_limits<int32_t>::max();
	#endif
	openNodes[index] = false;
	++closedNodes;
}

void AStarNodes::openNode(AStarNode* node)
{
	size_t index = node - nodes;
	assert(index < MAX_NODES);
	#if defined(__SSE2__)
	calculatedNodes[index] = nodes[index].f + nodes[index].g;
	#endif
	closedNodes -= (openNodes[index] ? 0 : 1);
	openNodes[index] = true;
}

int32_t AStarNodes::getClosedNodes() const
{
	return closedNodes;
}

AStarNode* AStarNodes::getNodeByPosition(uint32_t x, uint32_t y)
{
	uint32_t xy = (x << 16) | y;
	#if defined(__SSE2__)
	const __m128i key = _mm_set1_epi32(xy);

	int32_t pos = 0;
	int32_t curRound = curNode-16;
	for (; pos <= curRound; pos += 16) {
		__m128i v[4];
		v[0] = _mm_cmpeq_epi32(_mm_load_si128(reinterpret_cast<const __m128i*>(&nodesTable[pos])), key);
		v[1] = _mm_cmpeq_epi32(_mm_load_si128(reinterpret_cast<const __m128i*>(&nodesTable[pos+4])), key);
		v[2] = _mm_cmpeq_epi32(_mm_load_si128(reinterpret_cast<const __m128i*>(&nodesTable[pos+8])), key);
		v[3] = _mm_cmpeq_epi32(_mm_load_si128(reinterpret_cast<const __m128i*>(&nodesTable[pos+12])), key);
		const uint32_t mask = _mm_movemask_epi8(_mm_packs_epi16(_mm_packs_epi32(v[0], v[1]), _mm_packs_epi32(v[2], v[3])));
		if (mask != 0) {
			return &nodes[pos + _mm_ctz(mask)];
		}
	}
	curRound = curNode-8;
	if (pos <= curRound) {
		__m128i v[2];
		v[0] = _mm_cmpeq_epi32(_mm_load_si128(reinterpret_cast<const __m128i*>(&nodesTable[pos])), key);
		v[1] = _mm_cmpeq_epi32(_mm_load_si128(reinterpret_cast<const __m128i*>(&nodesTable[pos+4])), key);
		const uint32_t mask = _mm_movemask_epi8(_mm_packs_epi32(v[0], v[1]));
		if (mask != 0) {
			return &nodes[pos + (_mm_ctz(mask) >> 1)];
		}
		pos += 8;
	}
	for (; pos < curNode; ++pos) {
		if (nodesTable[pos] == xy) {
			return &nodes[pos];
		}
	}
	return nullptr;
	#else
	for (int32_t i = 1; i < curNode; ++i) {
		if (nodesTable[i] == xy) {
			return &nodes[i];
		}
	}
	return (nodesTable[0] == xy ? &nodes[0] : nullptr); // The first node is very unlikely to be the "neighbor" so leave it for end
	#endif
}

inline int_fast32_t AStarNodes::getMapWalkCost(AStarNode* node, const Position& neighborPos)
{
	//diagonal movement extra cost
	return (((std::abs(node->x - neighborPos.x) + std::abs(node->y - neighborPos.y)) - 1) * MAP_DIAGONALWALKCOST) + MAP_NORMALWALKCOST;
}

inline int_fast32_t AStarNodes::getTileWalkCost(const Creature& creature, const Tile* tile)
{
	int_fast32_t cost = 0;
	if (tile->getTopVisibleCreature(&creature) != nullptr) {
		//destroy creature cost
		cost += MAP_NORMALWALKCOST * 4;
	}
	if (const MagicField* field = tile->getFieldItem()) {
		CombatType_t combatType = field->getCombatType();
		if (!creature.isImmune(combatType) && !creature.hasCondition(Combat::DamageToConditionType(combatType)) && (creature.getMonster() && !creature.getMonster()->canWalkOnFieldType(combatType))) {
			cost += MAP_NORMALWALKCOST * 18;
		}
	}
	return cost;
}

// MapSector
bool MapSector::newSector = false;

MapSector::~MapSector()
{
	for (auto& depth : tiles) {
		for (auto& row : depth) {
			for (auto tile : row) {
				delete tile;
			}
		}
	}
}

void MapSector::createFloor(uint8_t z)
{
	floorBits |= (1 << static_cast<uint32_t>(z));
}

bool MapSector::getFloor(uint8_t z) const
{
	if (floorBits & (1 << static_cast<uint32_t>(z))) {
		return true;
	} else {
		return false;
	}
}

void MapSector::addCreature(Creature* c)
{
	creature_list.push_back(c);
	if (c->getPlayer()) {
		player_list.push_back(c);
	}
}

void MapSector::removeCreature(Creature* c)
{
	auto iter = std::find(creature_list.begin(), creature_list.end(), c);
	assert(iter != creature_list.end());
	*iter = creature_list.back();
	creature_list.pop_back();
	if (c->getPlayer()) {
		iter = std::find(player_list.begin(), player_list.end(), c);
		assert(iter != player_list.end());
		*iter = player_list.back();
		player_list.pop_back();
	}
}

uint32_t Map::clean() const
{
	uint64_t start = OTSYS_TIME();
	size_t tiles = 0;

	if (g_game.getGameState() == GAME_STATE_NORMAL) {
		g_game.setGameState(GAME_STATE_MAINTAIN);
	}

	std::vector<Item*> toRemove;
	toRemove.reserve(128);
	for (const auto& mit : mapSectors) {
		for (uint8_t z = 0; z < MAP_MAX_LAYERS; ++z) {
			if (mit.second.getFloor(z)) {
				for (auto& row : mit.second.tiles[z]) {
					for (auto tile : row) {
						if (!tile || tile->hasFlag(TILESTATE_PROTECTIONZONE)) {
							continue;
						}

						TileItemVector* itemList = tile->getItemList();
						if (!itemList) {
							continue;
						}

						++tiles;
						for (auto it = ItemVector::const_reverse_iterator(itemList->getEndDownItem()), end = ItemVector::const_reverse_iterator(itemList->getBeginDownItem()); it != end; ++it) {
							Item* item = (*it);
							if (item->isCleanable()) {
								toRemove.push_back(item);
							}
						}
					}
				}
			}
		}
	}

	size_t count = toRemove.size();
	for (Item* item : toRemove) {
		g_game.internalRemoveItem(item, -1);
	}
	toRemove.clear();

	if (g_game.getGameState() == GAME_STATE_MAINTAIN) {
		g_game.setGameState(GAME_STATE_NORMAL);
	}

	std::cout << "> CLEAN: Removed " << count << " item" << (count != 1 ? "s" : "")
	          << " from " << tiles << " tile" << (tiles != 1 ? "s" : "") << " in "
	          << (OTSYS_TIME() - start) / (1000.) << " seconds." << std::endl;
	return count;
}
