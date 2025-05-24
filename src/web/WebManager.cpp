#include "WebManager.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

WebManager::WebManager(WebConfig *config) {
	this->_webConfig = config;
	this->webServer = new ESP8266WebServer(this->_webConfig->port);
	this->mdns = new MDNSResponder();
}

WebManager::~WebManager() {
	delete this->webServer;
	delete this->mdns;
	delete this->_webConfig;
}

void WebManager::begin(const String &hostname) const {
	this->webServer->begin();
	this->mdns->begin(hostname);
	MDNS.addService("http", "tcp", this->_webConfig->port);
}

void WebManager::loop() {
	this->mdns->update();
	this->webServer->handleClient();
	if (const unsigned long now = millis(); now - lastCleanup >= 60000) { // 每分钟清理一次
		this->cleanupSessions();
		lastCleanup = now;
	}
}

void WebManager::on(const String &uri, const HTTPMethod method, const std::function<void()> &callback) const {
	this->webServer->on(uri, method, callback);
}

void WebManager::on(const String &uri, const HTTPMethod method, const std::function<void()> &callback,
					const std::function<void()> &u_function) const {
	this->webServer->on(uri, method, callback, u_function);
}

void WebManager::staticFS(const String &uri, const String &filename) const {
	this->webServer->serveStatic(uri.c_str(), LittleFS, filename.c_str());
}

void WebManager::onNotFound() const {
	this->webServer->onNotFound([this]() { this->webServer->send(404, "text/plain", "404: Not found"); });
}

bool WebManager::authenticate(const String &username, const String &password) const {
	return this->webServer->authenticate(username.c_str(), password.c_str());
}

void WebManager::cleanupSessions() {
	const unsigned long now = millis();
	for (auto it = activeSessions.begin(); it != activeSessions.end();) {
		if (it->second.expireAt <= now) {
			it = activeSessions.erase(it);
		} else {
			++it;
		}
	}
}

String WebManager::generateToken() {
	constexpr char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	String token;
	for (int i = 0; i < 32; ++i) {
		token += charset[random(0, sizeof(charset) - 1)];
	}
	return token;
}

bool WebManager::verify() const {
	if (this->_webConfig->username.isEmpty() || this->_webConfig->password.isEmpty()) {
		return true;
	}
	const String cookie = this->webServer->header("Cookie");
	if (const int idx = cookie.indexOf("AuthToken="); idx != -1) {
		String token = cookie.substring(idx + 10);
		if (const int sep = token.indexOf(';'); sep != -1) {
			token = token.substring(0, sep);
		}

		if (const auto it = activeSessions.find(token); it != activeSessions.end() && millis() < it->second.expireAt) {
			return true;
		}
	}
	return false;
}

void WebManager::handleFileRead(const String &path) const {
	const String contentType = "text/html";
	if (!LittleFS.exists(path)) {
		this->webServer->send(404, "application/json", "{error: Not found}");
		return;
	}

	File file = LittleFS.open(path, "r");
	if (!file) {
		this->webServer->send(500, "application/json", R"({"error": "File open failed"})");
		return;
	}

	this->webServer->streamFile(file, contentType);
	file.close();
}

void WebManager::defaultRoute() {
	this->webServer->collectHeaders("Cookie");

	this->onNotFound();
	// 默认主页重定向
	this->on("/", HTTP_GET, [this]() {
		this->webServer->sendHeader("Location", "/login.html");
		this->webServer->send(302);
	});

	this->on("/login.html", HTTP_GET, [this]() { handleFileRead("/login.html.gz"); });

	this->on("/config.html", HTTP_GET, [this]() {
		if (!this->verify()) {
			this->webServer->sendHeader("Location", "/");
			this->webServer->sendHeader("Cache-Control", "no-cache");
			this->webServer->sendHeader("Set-Cookie", "AuthToken=deleted");
			this->webServer->send(301);
			return;
		}
		handleFileRead("/config.html.gz");
	});

	this->on("/api/login", HTTP_POST, [this]() {
		String body = this->webServer->arg("plain");
		JsonDocument doc;
		deserializeJson(doc, body);

		const String user = doc["username"];
		const String pass = doc["password"];

		if (user == this->_webConfig->username && pass == this->_webConfig->password) {
			const String token = generateToken();
			const unsigned long expire = millis() + 3600000; // 有效期 1 小时

			activeSessions[token] = {token, expire};

			this->webServer->sendHeader("Set-Cookie", "AuthToken=" + token + "; Max-Age=3600; Path=/; HttpOnly");
			this->webServer->send(200, "application/json", R"({"status":"success"})");
		} else {
			this->webServer->send(403, "application/json", R"({"status":"error","message":"Unauthorized"})");
		}
	});

	this->on("/api/logout", HTTP_POST, [this]() {
		if (!this->verify()) {
			this->webServer->send(403, "application/json", R"({"status":"error","message":"Unauthorized"})");
			return;
		}
		const String cookie = this->webServer->header("Cookie");
		if (const int idx = cookie.indexOf("AuthToken="); idx != -1) {
			String token = cookie.substring(idx + 10);
			if (const int sep = token.indexOf(';'); sep != -1) {
				token = token.substring(0, sep);
			}
			activeSessions.erase(token);
		}
		this->webServer->sendHeader("Set-Cookie", "AuthToken=deleted; Path=/; Max-Age=0");
		this->webServer->send(200, "application/json", R"({"status":"logged_out"})");
	});

	this->on("/api/wifi", HTTP_GET, [this]() {
		if (!this->verify()) {
			this->webServer->send(403, "application/json", R"({"status":"error","message":"Unauthorized"})");
			return;
		}

		const auto wifi = WiFi.scanNetworks();
		if (wifi == 0) {
			this->webServer->send(200, "application/json", "[]");
			return;
		}

		JsonDocument doc;
		const JsonArray networks = doc.to<JsonArray>();
		for (int i = 0; i < wifi; i++) {
			auto network = networks.add<JsonObject>();
			network["ssid"] = WiFi.SSID(i);
			network["rssi"] = WiFi.RSSI(i);
		}

		this->webServer->send(200, "application/json", doc.as<String>());
		networks.clear();
		doc.clear();
	});

	this->on("/api/getConfig", HTTP_GET, [this]() {
		if (!this->verify()) {
			this->webServer->send(403, "application/json", R"({"status":"error","message":"Unauthorized"})");
			return;
		}
		if (!LittleFS.exists("/config.json")) {
			this->webServer->send(404, "application/json", "{error: Not found}");
			return;
		}
		File file = LittleFS.open("/config.json", "r");
		if (!file) {
			this->webServer->send(500, "application/json", "{error: Failed to open file}");
			return;
		}

		this->webServer->send(200, "application/json", file.readString());
		file.close();
	});

	this->on("/api/updateConfig", HTTP_POST, [this]() {
		if (!this->verify()) {
			this->webServer->send(403, "application/json", R"({"status":"error","message":"Unauthorized"})");
			return;
		}
		if (!LittleFS.exists("/config.json")) {
			this->webServer->send(404, "application/json", "{error: Not found}");
			return;
		}
		if (!this->webServer->hasArg("plain")) {
			this->webServer->send(400, "application/json", R"({"status":"error","message":"Invalid JSON payload."})");
			return;
		}
		const String jsonPayload = this->webServer->arg("plain");
		JsonDocument doc; // 根据实际情况调整大小
		const DeserializationError error = deserializeJson(doc, jsonPayload);
		if (error) {
			String errorMessage = "Parse error: ";
			errorMessage += error.c_str();
			this->webServer->send(400, "application/json", R"({"status":"error","message":")" + errorMessage + "\"}");
			return;
		}

		// 提取配置数据
		BeginSettings settings;

		// 解析WiFi配置
		if (doc["wifi"].is<JsonObject>()) {
			JsonObject wifi = doc["wifi"];
			settings.wifiConfig.ssid = wifi["ssid"].as<String>();
			settings.wifiConfig.pass = wifi["pass"].as<String>();
			settings.wifiConfig.hostname = wifi["hostname"].as<String>();
		} else {
			this->webServer->send(400, "application/json", R"({"status":"error","message":"Missing wifiConfig"})");
			return;
		}

		// 解析HeFeng配置
		if (doc["hefeng"].is<JsonObject>()) {
			JsonObject heFeng = doc["hefeng"];
			settings.heFengConfig.key = heFeng["key"].as<String>();
			settings.heFengConfig.location = heFeng["location"].as<String>();
			settings.heFengConfig.baseUrl = heFeng["baseUrl"].as<String>();
		} else {
			this->webServer->send(400, "application/json", R"({"status":"error","message":"Missing heFengConfig"})");
			return;
		}

		// 解析Web配置
		if (doc["web"].is<JsonObject>()) {
			JsonObject web = doc["web"];
			settings.webConfig.port = web["port"].as<uint16_t>();
			settings.webConfig.username = web["username"].as<String>();
			settings.webConfig.password = web["password"].as<String>();
		} else {
			this->webServer->send(400, "application/json", R"({"status":"error","message":"Missing webConfig"})");
			return;
		}

		// 执行详细验证
		String validationErrors;
		if (!settings.wifiConfig.validate()) {
			validationErrors += "WiFi configuration check failed. (SSID/Password/Hostname format error)";
		}
		if (!settings.heFengConfig.validate()) {
			validationErrors += "HeFen configuration check failed. (ApiKey/Location/BaseUrl format error)";
		}
		if (!settings.webConfig.validate()) {
			validationErrors += "Web configuration check failed. (Port/Username/Password format error)";
		}

		if (!validationErrors.isEmpty()) {
			this->webServer->send(422, "application/json",
								  R"({"status":"error","message":")" + validationErrors + "\"}");
			return;
		}

		File configFile = LittleFS.open("/config.json", "w");
		if (!configFile) {
			this->webServer->send(500, "application/json", R"({"status":"error","message":"Failed to open file"})");
			return;
		}

		serializeJsonPretty(doc, configFile);
		configFile.close();

		this->webServer->send(200, "application/json", R"({"status":"success"})");
	});
}
