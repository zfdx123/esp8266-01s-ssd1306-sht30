
#pragma once
#include <ArduinoJson.h>

typedef struct HeFengCurrentData {
	String cond_txt;
	String fl;
	String tmp;
	String hum;
	String wind_sc;
	String iconMeteoCon;
	String follower;
} HeFengCurrentData;

typedef struct HeFengForeData {
	String datestr;
	String tmp_min;
	String tmp_max;
	String iconMeteoCon;
} HeFengForeData;

class HeFeng {
private:
	static String getMeteoconIcon(const String &cond_code);
	static bool httpsGet(const String &url, String &response);
	static void setDefaultWeatherData(HeFengCurrentData *data);
	static void setDefaultForecastData(HeFengForeData *data);
	static String Decode(StreamString in);

public:
	HeFeng();
	static void doUpdateCurr(HeFengCurrentData *data, const String &key, const String &location);
	static void doUpdateFore(HeFengForeData *data, const String &key, const String &location);
};
