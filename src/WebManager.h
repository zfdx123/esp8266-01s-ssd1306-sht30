//
// Created by 29154 on 2025/5/21.
//

#ifndef WEBMANAGER_H
#define WEBMANAGER_H

#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "Begin.h"

class WebManager {
private:
	struct Session {
		String token;
		unsigned long expireAt; // 单位: millis()
	};
	std::map<String, Session> activeSessions; // token => Session

	unsigned long lastCleanup = 0;

	WebConfig* _webConfig;
	ESP8266WebServer* webServer;
	MDNSResponder* mdns;
	[[nodiscard]] bool verify() const;
	void cleanupSessions();
	void handleFileRead(const String& path) const;

public:
	explicit WebManager(WebConfig* config);
	void begin(const String& hostname) const;
	void loop();
	void on(const String &uri, HTTPMethod method, const std::function<void()> &callback) const;
	void staticFS(const String &uri, const String &filename) const;
	void onNotFound() const;
	[[nodiscard]] bool authenticate(const String &username, const String &password) const;
	[[nodiscard]] static String generateToken() ;
	void defaultRoute();
};

#endif //WEBMANAGER_H
