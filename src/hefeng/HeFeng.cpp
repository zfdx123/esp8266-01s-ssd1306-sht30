#include <ArduinoUZlib.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>

#include "HeFeng.h"

#include <map>

HeFeng::HeFeng(HeFengConfig *config) {
	this->_heFengConfig = config;
	this->client = new BearSSL::WiFiClientSecure();
	this->client->setInsecure();
}

String HeFeng::Decode(StreamString in) {
	const size_t size = in.available();
	uint8_t buf[size];
	in.readBytes(buf, size);
	uint8_t *outBuf = nullptr;
	uint32_t outSize = 0;
	ArduinoUZlib::decompress(buf, size, outBuf, outSize);
	String result = {reinterpret_cast<char *>(outBuf)};
	free(outBuf);
	return result;
}

void HeFeng::doUpdateCurr(HeFengCurrentData *data) const {
	const String url = "/v7/weather/now?location=" + this->_heFengConfig->location;
	Serial.println("[HTTPS] Fetching weather forecast data...");

	if (String response; this->httpsGet(url, response)) {
		JsonDocument jsonBuffer;

		if (const DeserializationError error = deserializeJson(jsonBuffer, HeFeng::Decode(response)); !error) {
			const JsonObject root = jsonBuffer["now"];
			data->tmp = root["temp"] | "-1";
			data->fl = root["feelsLike"] | "-1";
			data->hum = root["humidity"] | "-1";
			data->wind_sc = root["windScale"] | "-1";
			data->iconMeteoCon = getMeteoConIcon(root["icon"] | "999");
			data->cond_txt = root["pressure"] | "-1";
			root.clear();
			jsonBuffer.clear();
		} else {
			Serial.println("[JSON] Parse error!");
			setDefaultWeatherData(data);
		}
	} else {
		setDefaultWeatherData(data);
	}
}

void HeFeng::setDefaultWeatherData(HeFengCurrentData *data) {
	data->tmp = "-1";
	data->fl = "-1";
	data->hum = "-1";
	data->wind_sc = "-1";
	data->cond_txt = "no network";
	data->iconMeteoCon = ")";
}

void HeFeng::doUpdateFore(HeFengForeData *data) const {
	const String url = "/v7/weather/3d?lang=en&&location=" + this->_heFengConfig->location;
	Serial.println("[HTTPS] Fetching weather forecast data...");

	if (String response; this->httpsGet(url, response)) {
		JsonDocument jsonBuffer;
		if (const DeserializationError error = deserializeJson(jsonBuffer, HeFeng::Decode(response)); !error) {
			const JsonArray forecasts = jsonBuffer["daily"];
			for (int i = 0; i < 3; i++) {
				JsonObject day = forecasts[i];
				data[i].tmp_min = day["tempMin"] | "-1";
				data[i].tmp_max = day["tempMax"] | "-1";
				data[i].datestr = day["fxDate"].as<String>().substring(5);
				String cond_code = day["iconDay"] | "999";
				data[i].iconMeteoCon = getMeteoConIcon(cond_code);
			}
			forecasts.clear();
			jsonBuffer.clear();
		} else {
			Serial.println("[JSON] Parse error!");
			setDefaultForecastData(data);
		}
	} else {
		setDefaultForecastData(data);
	}
}

void HeFeng::setDefaultForecastData(HeFengForeData *data) {
	for (int i = 0; i < 3; i++) {
		data[i].tmp_min = "-1";
		data[i].tmp_max = "-1";
		data[i].datestr = "N/A";
		data[i].iconMeteoCon = ")";
	}
}

bool HeFeng::httpsGet(const String &url, String &response) const {
	HTTPClient https;
	https.setReuse(false);

	if (!https.begin(*this->client, this->_heFengConfig->baseUrl + url)) {
		Serial.println("[HTTPS] Unable to connect");
		return false;
	}

	https.addHeader("Accept-Encoding", "gzip");
	https.addHeader("X-QW-Api-Key", this->_heFengConfig->key);
	https.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/115.0");

	int httpCode = https.GET();
	if (httpCode > 0) {
		if (httpCode == HTTP_CODE_OK) {
			response = https.getString();
			https.end();
			this->client->stop();
			return true;
		}
		Serial.printf("[HTTPS] GET returned httpCode=%d\n", httpCode);
	} else {
		Serial.printf("[HTTPS] GET failed, error: %s\n", HTTPClient::errorToString(httpCode).c_str());
	}

	https.end();
	this->client->stop();
	return false;
}


String HeFeng::getMeteoConIcon(const String &cond_code) {
	static const std::map<String, String> iconMap = {
			{"100", "B"}, {"9006", "B"}, {"999", ")"}, {"104", "D"}, {"500", "E"}, {"503", "F"}, {"504", "F"},
			{"507", "F"}, {"508", "F"},	 {"499", "G"}, {"901", "G"}, {"103", "H"}, {"502", "L"}, {"511", "L"},
			{"512", "L"}, {"513", "L"},	 {"501", "M"}, {"509", "M"}, {"510", "M"}, {"514", "M"}, {"515", "M"},
			{"102", "N"}, {"213", "O"},	 {"302", "P"}, {"303", "P"}, {"305", "Q"}, {"308", "Q"}, {"309", "Q"},
			{"314", "Q"}, {"399", "Q"},	 {"306", "R"}, {"307", "R"}, {"310", "R"}, {"311", "R"}, {"312", "R"},
			{"315", "R"}, {"316", "R"},	 {"317", "R"}, {"318", "R"}, {"200", "S"}, {"201", "S"}, {"202", "S"},
			{"203", "S"}, {"204", "S"},	 {"205", "S"}, {"206", "S"}, {"207", "S"}, {"208", "S"}, {"209", "S"},
			{"210", "S"}, {"211", "S"},	 {"212", "S"}, {"300", "T"}, {"301", "T"}, {"400", "U"}, {"408", "U"},
			{"407", "V"}, {"401", "W"},	 {"402", "W"}, {"403", "W"}, {"409", "W"}, {"410", "W"}, {"304", "X"},
			{"313", "X"}, {"404", "X"},	 {"405", "X"}, {"406", "X"}, {"101", "Y"}, {"150", "C"}, {"151", "I"},
			{"152", "K"}, {"153", "L"}, {"350", "!"}, {"351", "$"}, {"456", "\""}, {"457", "#"}};
	const auto it = iconMap.find(cond_code);
	return it != iconMap.end() ? it->second : ")";
}
