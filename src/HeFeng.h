
#pragma once
#include <ArduinoJson.h>
#include <StreamString.h>
#include "Begin.h"

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
	HeFengConfig *_heFengConfig;
	static String getMeteoConIcon(const String &cond_code);
	bool httpsGet(const String &url, String &response) const;
	static void setDefaultWeatherData(HeFengCurrentData *data);
	static void setDefaultForecastData(HeFengForeData *data);
	static String Decode(StreamString in);

public:
	explicit HeFeng(HeFengConfig *config);
	void doUpdateCurr(HeFengCurrentData *data) const;
	void doUpdateFore(HeFengForeData *data) const;
};
