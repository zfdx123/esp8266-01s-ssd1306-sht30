//
// Created by 29154 on 2025/5/1.
//

#ifndef BEGIN_H
#define BEGIN_H

/***************************
   Begin Settings
 **************************/
typedef struct WifiConfig {
	String ssid;
	String pass;
	String hostname;

	[[nodiscard]] bool validate() const {
		return ssid.length() > 0 && ssid.length() <= 32 &&
			   pass.length() <= 64 &&
			   hostname.length() <= 32;
	}
} WifiConfig;

typedef struct HeFengConfig {
	String key;
	String location;
	String baseUrl;

	[[nodiscard]] bool validate() const {
		return key.length() == 32 &&  // 假设key是32位
			   location.length() == 9 &&  // 假设location是9位
			   baseUrl.startsWith("http");
	}
} HeFengConfig;

typedef struct WebConfig {
	uint16_t port;
	String username;
	String password;

	[[nodiscard]] bool validate() const {
		return port > 0 && port <= 65535 &&
			   username.length() >= 4 && username.length() <= 20 &&
			   password.length() >= 6 && password.length() <= 32;
	}
} WebConfig;

typedef struct BeginSettings {
	WifiConfig wifiConfig;
	HeFengConfig heFengConfig;
	WebConfig webConfig;

	[[nodiscard]] bool validate() const {
		return wifiConfig.validate() &&
			   heFengConfig.validate() &&
			   webConfig.validate();
	}
} BeginSettings;

#endif //BEGIN_H
