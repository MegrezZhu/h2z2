#pragma once

#include "cocos2d.h"

class Boom : public cocos2d::ParticleSystemQuad {
	Boom();
public:
	static Boom* create(cocos2d::Vec2 pos);
};
