﻿#include "GameScene.h"
#include "utils\global.h"
#include <chrono>
#include <algorithm>

USING_NS_CC;
using namespace std::chrono;

Scene* GameScene::createScene() {
	// 'scene' is an autorelease object
	auto scene = Scene::createWithPhysics();

	scene->getPhysicsWorld()->setAutoStep(false);
	scene->getPhysicsWorld()->setGravity(Vec2::ZERO);

	// 'layer' is an autorelease object
	auto gameLayer = GameScene::create();
	gameLayer->mWorld = scene->getPhysicsWorld();
	gameLayer->uiLayer = Layer::create();

	// add layer as a child to scene
	scene->addChild(gameLayer);
	scene->addChild(gameLayer->uiLayer, 1);

	// return the scene
	return scene;
}

// on "init" you need to initialize your instance
bool GameScene::init() {
	//////////////////////////////
	// 1. super init first
	if (!Layer::init()) {
		return false;
	}

	this->addListener();

	this->visibleSize = Director::getInstance()->getVisibleSize();
	this->origin = Director::getInstance()->getVisibleOrigin();

	auto background = Sprite::create("background.png");
	gameArea = background->getContentSize();
	Bullet::initAutoRemove(gameArea);
	background->setPosition(gameArea / 2);
	this->addChild(background, -1);
	addChild(Weapons::create(0, "123456", Vec2(gameArea.x / 2 - 100, gameArea.y / 2)));
	addChild(Weapons::create(1, "123457", Vec2(gameArea.x / 2, gameArea.y / 2)));

	// received as the game starts
	GSocket->on("initData", [=](GameSocket* client, GenericValue<UTF8<>>& data) {
		hpLabel = Label::createWithSystemFont("", "Arial", 30);
		hpLabel->setPosition(visibleSize.width / 2, 20.0f);
		uiLayer->addChild(hpLabel);
		weaponLabel = Label::createWithSystemFont("", "Arial", 30);
		weaponLabel->setPosition(visibleSize.width - 70.0f, 20.0f);
		uiLayer->addChild(weaponLabel);
		this->selfId = data["selfId"].GetString();
		auto& arr = data["players"];
		for (SizeType i = 0; i < arr.Size(); i++) {
			const std::string& id = arr[i].GetString();
			if (id == selfId) {
				auto& selfPos = data["selfPos"];
				selfPlayer = Player::create(Vec2(selfPos["x"].GetDouble(), selfPos["y"].GetDouble()));
				this->addChild(selfPlayer, 1);
				updateHpLabel();

				// make the camera follow the player
				this->runAction(Follow::create(selfPlayer));
			} else {
				auto player = Player::create();
				this->addChild(player, 1);
				this->otherPlayers.insert(std::make_pair(id, player));
			}
		}

		auto& packs = data["healPacks"];
		for (SizeType i = 0; i < packs.Size(); i++) {
			auto pack = HealPack::create(packs[i]);
			this->addChild(pack, 0);
		}

		started = true;
	});

	// received periodically, like every x frames
	GSocket->on("sync", [=](GameSocket* client, GenericValue<UTF8<>>& arr) {
		for (SizeType i = 0; i < arr.Size(); i++) {
			auto& data = arr[i]["data"];

			// check ping
			// milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
			// CCLOG("ping: %lld", ms.count() - data["timestamp"].GetInt64());

			const std::string& id = arr[i]["id"].GetString();
			auto it = this->otherPlayers.find(id);
			if (it == this->otherPlayers.end()) continue; // data of selfPlayer, just ignore it

			auto player = it->second;
			player->sync(data);
		}
	});

	// deal with other players' shots
	GSocket->on("bullet", [=](GameSocket* client, GenericValue<UTF8<>>& data) {
		auto file = data["file"].GetString();
		auto pos = Vec2(data["posX"].GetDouble(), data["posY"].GetDouble());
		auto angle = data["angle"].GetDouble();
		auto speed = data["speed"].GetDouble();
		addChild(new Bullet(file, pos, angle, speed));
	});

	GSocket->on("hit", [=](GameSocket* client, GenericValue<UTF8<>>& data) {
		auto dmg = data["damage"].GetDouble();
		auto player = getPlayerById(data["from"].GetString());
		if (player != nullptr) player->damage(dmg);
	});

	GSocket->on("dead", [=](GameSocket* client, GenericValue<UTF8<>>& data) {
		auto who = data["from"].GetString();
		auto it = otherPlayers.find(who);
		if (it != otherPlayers.end()) {
			it->second->removeFromParentAndCleanup(true);
			otherPlayers.erase(it);
		}
	});

	GSocket->on("eatPack", [=](GameSocket* client, GenericValue<UTF8<>>& data) {
		HealPack::remove(data["packId"].GetString());
	});

	GSocket->on("takeWeapon", [=](GameSocket* client, GenericValue<UTF8<>>& data) {
		auto player = getPlayerById(data["from"].GetString());
		auto weapon = Weapons::getById(data["weaponId"].GetString());
		if (player != nullptr && weapon != nullptr) {
			player->takeWeapon(weapon);
		}
	});

	GSocket->on("dropWeapon", [=](GameSocket* client, GenericValue<UTF8<>>& data) {
		auto player = getPlayerById(data["from"].GetString());
		if (player != nullptr) {
			this->addChild(player->dropWeapon());
		}
	});

	// manually update the physics world
	this->schedule(schedule_selector(GameScene::update), 1.0f / FRAME_RATE, kRepeatForever, 0.1f);
	return true;
}

void GameScene::update(float dt) {
	using std::max;
	using std::min;

	if (!started) return;
	static int frameCounter = 0;
	this->getScene()->getPhysicsWorld()->step(1.0f / FRAME_RATE);
	frameCounter++;

	if (!selfPlayer) return;
	if (frameCounter == SYNC_LIMIT) {
		frameCounter = 0;
		GSocket->sendEvent("sync", this->selfPlayer->createSyncData());
	}

	// bound the player in the map
	auto pos = selfPlayer->getPosition();
	auto size = selfPlayer->getContentSize() * selfPlayer->getScale();
	pos.x = min(pos.x, gameArea.x - size.width / 2);
	pos.x = max(pos.x, size.width / 2);
	pos.y = min(pos.y, gameArea.y - size.height / 2);
	pos.y = max(pos.y, size.height / 2);
	selfPlayer->setPosition(pos);
}

void GameScene::onKeyPressed(EventKeyboard::KeyCode code, Event* event) {
	switch (code) {
		case cocos2d::EventKeyboard::KeyCode::KEY_W:
			selfPlayer->setVelocityY(200.0f);
			break;
		case cocos2d::EventKeyboard::KeyCode::KEY_S:
			selfPlayer->setVelocityY(-200.0f);
			break;
		case cocos2d::EventKeyboard::KeyCode::KEY_A:
			selfPlayer->setVelocityX(-200.0f);
			break;
		case cocos2d::EventKeyboard::KeyCode::KEY_D:
			selfPlayer->setVelocityX(200.0f);
			break;
		case cocos2d::EventKeyboard::KeyCode::KEY_G:
			updateWeaponLabel();
			auto w = selfPlayer->dropWeapon();
			if (w != nullptr) {
				this->addChild(w);
				w->broadCastDropped();
			}
			break;
	}
}

void GameScene::onKeyReleased(EventKeyboard::KeyCode code, Event* event) {
	auto body = selfPlayer->getPhysicsBody();
	switch (code) {
		case cocos2d::EventKeyboard::KeyCode::KEY_W:
			if (body->getVelocity().y < 0.0f) break;
			selfPlayer->setVelocityY(0.0f);
			break;
		case cocos2d::EventKeyboard::KeyCode::KEY_S:
			if (body->getVelocity().y > 0.0f) break;
			selfPlayer->setVelocityY(0.0f);
			break;
		case cocos2d::EventKeyboard::KeyCode::KEY_A:
			if (body->getVelocity().x > 0.0f) break;
			selfPlayer->setVelocityX(0.0f);
			break;
		case cocos2d::EventKeyboard::KeyCode::KEY_D:
			if (body->getVelocity().x < 0.0f) break;
			selfPlayer->setVelocityX(0.0f);
			break;
	}
}

void GameScene::onMouseMove(EventMouse* event) {
	auto pos = Vec2(event->getCursorX(), event->getCursorY());
	// position of physicsBody as relative to the camera
	selfPlayer->setRotation(-CC_RADIANS_TO_DEGREES((pos - selfPlayer->getPhysicsBody()->getPosition()).getAngle()) + 90.0f);
}

void GameScene::onMouseDown(EventMouse* event) {
	switch (event->getMouseButton()) {
		case EventMouse::MouseButton::BUTTON_LEFT:
			if (selfPlayer->weapon != nullptr) {
				selfPlayer->weapon->fire();
			}
			updateWeaponLabel();
			break;
		case EventMouse::MouseButton::BUTTON_RIGHT:
		default:
			break;
	}
}

bool GameScene::onContactBegin(PhysicsContact &contact) {
	auto node1 = (Sprite*)contact.getShapeA()->getBody()->getNode();
	auto node2 = (Sprite*)contact.getShapeB()->getBody()->getNode();

	// handle player-bullet collision
	if (_handleContact<Player, Bullet>(node1, node2)) return false;
	// handle player-healpack collision
	if (_handleContact<Player, HealPack>(node1, node2)) return false;
	// handle player-weapon collision
	if (_handleContact<Player, Weapon>(node1, node2)) return false;

	return false;
}

void GameScene::addListener() {
	auto keyboardListener = EventListenerKeyboard::create();
	keyboardListener->onKeyPressed = CC_CALLBACK_2(GameScene::onKeyPressed, this);
	keyboardListener->onKeyReleased = CC_CALLBACK_2(GameScene::onKeyReleased, this);

	auto mouseListener = EventListenerMouse::create();
	mouseListener->onMouseMove = CC_CALLBACK_1(GameScene::onMouseMove, this);
	mouseListener->onMouseDown = CC_CALLBACK_1(GameScene::onMouseDown, this);

	auto contactListener = EventListenerPhysicsContact::create();
	contactListener->onContactBegin = CC_CALLBACK_1(GameScene::onContactBegin, this);

	_eventDispatcher->addEventListenerWithSceneGraphPriority(keyboardListener, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(mouseListener, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(contactListener, this);
}

void GameScene::handleContact(Player* player, Bullet* bullet) {
	auto boom = Boom::create(player->getPosition());
	this->addChild(boom, 2);
	bullet->removeFromParentAndCleanup(true);
	if (player == selfPlayer) {
		// self-player was hit
		player->broadcastHit(20.0f);
		if (!player->damage(20.0f)) {
			player->broadcastDead();
			gameOver();
		}
		updateHpLabel();
	}
}

void GameScene::handleContact(Player* player, HealPack* pack) {
	player->heal(pack->getHp());
	updateHpLabel();
	pack->broadcastEaten();
	HealPack::remove(pack->getId());
}

void GameScene::handleContact(Player* player, Weapon* weapon) {
	if (player == selfPlayer && selfPlayer->weapon == nullptr) {
		player->takeWeapon(weapon);
		weapon->broadCastToken();
		updateWeaponLabel();
	}
}

void GameScene::gameOver() {
	_eventDispatcher->removeEventListenersForType(EventListener::Type::KEYBOARD);
	_eventDispatcher->removeEventListenersForType(EventListener::Type::MOUSE);
	alive = false;
	// invisible & untouchable
	auto body = selfPlayer->getPhysicsBody();
	selfPlayer->setVisible(false);
	body->setCollisionBitmask(0x00000000);
	body->setContactTestBitmask(0x00000000);
	body->setCategoryBitmask(0x00000000);
}

void GameScene::updateHpLabel() {
	static char buffer[20];
	sprintf(buffer, "hp: %d", selfPlayer->getHp());
	hpLabel->setString(buffer);
}

void GameScene::updateWeaponLabel() {
	static char buffer[40];
	auto w = selfPlayer->weapon;
	if (w == nullptr) {
		weaponLabel->setString("");
	} else {
		sprintf(buffer, "gun: %d/%d", w->getCurrent(), w->getMagazine());
		weaponLabel->setString(buffer);
	}
}

template<typename Type1, typename Type2>
bool GameScene::_handleContact(cocos2d::Sprite *node1, cocos2d::Sprite *node2) {
	if (isType<Type1>(node1) && isType<Type2>(node2)) {
		handleContact(castType<Type1>(node1), castType<Type2>(node2));
		return true;
	}
	if (isType<Type1>(node2) && isType<Type2>(node1)) {
		handleContact(castType<Type1>(node2), castType<Type2>(node1));
		return true;
	}
	return false;
}

void resetPhysics(Node* node, PhysicsBody* body) {
	node->removeComponent(node->getPhysicsBody());
	node->setPhysicsBody(body);
}

Player* GameScene::getPlayerById(std::string id) {
	auto it = otherPlayers.find(id);
	if (it != otherPlayers.end()) return it->second;
	return nullptr;
}
