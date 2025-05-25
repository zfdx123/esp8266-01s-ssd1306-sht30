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
		return ssid.length() > 0 && ssid.length() <= 32 && pass.length() <= 64 && hostname.length() <= 32;
	}
} WifiConfig;

typedef struct HeFengConfig {
	String key;
	String location;
	String baseUrl;

	[[nodiscard]] bool validate() const {
		// 验证key是32位
		if (key.length() != 32) {
			return false;
		}

		// 验证location是9位数字或经纬度格式
		bool locationValid = false;
		if (location.length() == 9) {
			// 检查是否全是数字
			locationValid = true;
			for (char c : location) {
				if (!isdigit(c)) {
					locationValid = false;
					break;
				}
			}
		} else {
			// 检查是否是经纬度格式 "经度,纬度"
			if (const int commaPos = location.indexOf(','); commaPos != -1) {
				const String longitude = location.substring(0, commaPos);
				const String latitude = location.substring(commaPos + 1);
				// 简单验证 - 可以根据需要添加更复杂的验证
				locationValid = longitude.length() > 0 && latitude.length() > 0;
			}
		}

		// 验证baseUrl以http开头
		if (!baseUrl.startsWith("http")) {
			return false;
		}

		return locationValid;
	}
} HeFengConfig;

typedef struct WebConfig {
	uint16_t port;
	String username;
	String password;

	[[nodiscard]] bool validate() const {
		return port > 0 && port <= 65535 && username.length() >= 4 && username.length() <= 20 &&
			   password.length() >= 6 && password.length() <= 32;
	}
} WebConfig;

typedef struct BeginSettings {
	WifiConfig wifiConfig;
	HeFengConfig heFengConfig;
	WebConfig webConfig;

	[[nodiscard]] bool validate() const {
		return wifiConfig.validate() && heFengConfig.validate() && webConfig.validate();
	}
} BeginSettings;

#endif // BEGIN_H
