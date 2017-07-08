#pragma once
#include "Pistol.h"
#include "Uzi.h"

class Weapons {
private:
	Weapons();
	static std::map<std::string, Weapon*> pool;
public:
	static Weapon* create(int type, std::string id, cocos2d::Vec2 pos);
	static Weapon* getById(std::string id);
};
