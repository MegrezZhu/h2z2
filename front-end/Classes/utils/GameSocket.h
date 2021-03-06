#pragma once

#include "network\WebSocket.h"
#include "cocos2d\external\json\document.h"
#include "cocos2d\external\json\writer.h"
#include "cocos2d\external\json\stringbuffer.h"
#include "cocos2d\external\json\rapidjson.h"
#include <functional>
#include <map>
#include <string>

using namespace cocos2d::network;
using namespace rapidjson;

class GameSocket : public WebSocket::Delegate {
	static GameSocket* instance;
	WebSocket* socket;
	std::map<std::string, std::function<void(GameSocket*, GenericValue<UTF8<>>&)>> eventPool;
	std::function<void(GameSocket*)> connectionCallback;
	static std::string stringifyDom(const Document& dom);

	GameSocket();
	GameSocket(std::string host, std::string port);
	void onOpen(WebSocket* ws) override;
	void onMessage(WebSocket* ws, const WebSocket::Data &data);
	void onClose(WebSocket* ws) override;
	void onError(WebSocket* ws, const WebSocket::ErrorCode &data);
public:
	static GameSocket* getInstance();
	static void init(std::string host, std::string port);

	void sendEvent(const std::string& eventName, GenericValue<UTF8<>>& dom);
	void sendEvent(const std::string& eventName);
	void on(const std::string& eventName, std::function<void(GameSocket*, GenericValue<UTF8<>>&)> fn);
	void removeEventHandler(const std::string& eventName);

	void onConnection(std::function<void(GameSocket*)> fn);
};

#define GSocket GameSocket::getInstance()