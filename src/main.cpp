#include <Arduino.h>

#include <ESP8266WebServer.h>

#include <ctime>
#include <sys/time.h>

#include <OLEDDisplayUi.h>
#include <SSD1306Wire.h> //0.96寸用这个

#include <Wire.h>
#include "SHT30.h"

#include <FS.h>
#include <LittleFS.h>
#include "HeFeng.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"

/***************************
   Begin Settings
 **************************/
typedef struct WifiConfig {
	String ssid;
	String pass;
} WifiConfig;

typedef struct HeFengConfig {
	String key;
	String location;
} HeFengConfig;

typedef struct WebConfig {
	bool enable;
	uint16_t port;
} WebConfig;

typedef struct BeginSettings {
	WifiConfig wifiConfig;
	HeFengConfig heFengConfig;
	WebConfig webConfig{};
} BeginSettings;

#define TZ 8 // 中国时区为8
#define DST_MN 0 // 默认为0

constexpr int UPDATE_INTERVAL_SECS = 5 * 60; // 5分钟更新一次天气
constexpr int UPDATE_CURR_INTERVAL_SECS = 60 * 60; // 2分钟更新一次粉丝数
constexpr int UPDATE_SHT30_INTERVAL_SECS = 1; // 5秒更新一次SHT30

constexpr int SHT31_ADDRESS = 0x44;
constexpr int I2C_DISPLAY_ADDRESS = 0x3c; // I2c地址默认
#if defined(ESP8266)
constexpr int SDA_PIN = 0; // 引脚连接
constexpr int SDC_PIN = 2; //
#endif

const String WDAY_NAMES[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"}; // 星期
const String MONTH_NAMES[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
							  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"}; // 月份

/***************************
   End Settings
 **************************/

// SH1106Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);   // 1.3寸用这个
SSD1306Wire display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN); // 0.96寸用这个
OLEDDisplayUi ui(&display);

SHT30 sht30(SHT31_ADDRESS, SDA_PIN, SDC_PIN);

HeFengCurrentData currentWeather; // 实例化对象
HeFengForeData foreWeather[3];
SHT30Data sht30Data;
BeginSettings beginSettings;
ESP8266WebServer *server;

#define TZ_MN ((TZ) * 60) // 时间换算
#define TZ_SEC ((TZ) * 3600)
#define DST_SEC ((DST_MN) * 60)

time_t now; // 实例化时间

bool readyForWeatherUpdate = false; // 天气更新标志
bool first = true; // 首次更新标志
bool configStatus = false; // 配置状态标志
unsigned long timeSinceLastWUpdate = 0; // 上次更新后的时间
unsigned long timeSinceLastCurrUpdate = 0; // 上次天气更新后的时间
unsigned long timeSinceLastSHT30Update = 0; // 上次SHT30更新后的时间

void drawProgress(OLEDDisplay *display, int percentage, const String& label); // 提前声明函数
void updateData(OLEDDisplay *display);
void updateDataAll(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawDeviceInfo(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state);
void drawHeaderOverlayTmp(OLEDDisplay *display, OLEDDisplayUiState *state);
void setReadyForWeatherUpdate();
bool loadConfig(BeginSettings &beginSettings);
bool saveConfig(const String &data);
bool wifiConnect();
int16_t add_draw(int arg1, int arg2);
void setMode();
String readConfig();
void webServer();
void displayStatus(int16_t x, int16_t y,const String &msg);

// 添加框架
// 此数组保留指向所有帧的函数指针
// 框架是从右向左滑动的单个视图
FrameCallback frames[] = {drawDateTime, drawCurrentWeather, drawForecast, drawDeviceInfo};
// 页面数量
int numberOfFrames = 4;

// 覆盖层回调
OverlayCallback dynamicOverlay[] = {drawHeaderOverlay, drawHeaderOverlayTmp}; // 当前动态覆盖层
uint8_t overlayIndex[] = {1, 0, 0, 0};
// 当前页面索引
int currentFrameIndex = -1; // 初始化为 -1，强制刷新覆盖层

void setup() {
	Serial.begin(115200);
	Serial.println();
	Serial.println();

	// 初始化SHT30
	sht30.begin();

	// 屏幕初始化
	display.init();
	display.clear();
	display.display();

	display.flipScreenVertically(); // 屏幕翻转
	display.setContrast(255); // 屏幕亮度


	if (loadConfig(beginSettings)) {
		configStatus = true;
		displayStatus(2, 0,"Config loaded OK.");
		Serial.println("Config loaded successfully.");
	} else {
		displayStatus(2, 0,"Config loaded Failed.");
	}

	if (beginSettings.webConfig.enable) {
		if (!configStatus){
			setMode();
		}
		webServer();
		displayStatus(2, 12,"Web Start OK.");
		displayStatus(2, 36, WiFi.softAPIP().toString());
	} else {
		if (server != nullptr) {
			server->stop();
			delete server;
			server = nullptr;
		}
		displayStatus(2, 12,"Web Not Start.");
	}

	if (!configStatus) {
		return;
	}

	// 连接WiFi
	if (!wifiConnect()) {
		configStatus = false;
		setMode();
		displayStatus(2, 24,"WiFi Connect Failed.");
		displayStatus(2, 36,WiFi.softAPIP().toString());
		return;
	}

	ui.setTargetFPS(30); // 刷新频率
	ui.setActiveSymbol(activeSymbole); // 设置活动符号
	ui.setInactiveSymbol(inactiveSymbole); // 设置非活动符号

	// 符号位置
	// 你可以把这个改成TOP, LEFT, BOTTOM, RIGHT
	ui.setIndicatorPosition(BOTTOM);

	// 定义第一帧在栏中的位置
	ui.setIndicatorDirection(LEFT_RIGHT);

	// 屏幕切换方向
	// 您可以更改使用的屏幕切换方向 SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
	ui.setFrameAnimation(SLIDE_LEFT);

	ui.setFrames(frames, numberOfFrames); // 设置框架
	ui.setTimePerFrame(5000); // 设置切换时间

	ui.setOverlays(dynamicOverlay, overlayIndex); // 设置覆盖

	// UI负责初始化显示
	ui.init();
	display.flipScreenVertically(); // 屏幕反转

	configTime(
			TZ_SEC, DST_SEC, "ntp.ntsc.ac.cn",
			"ntp1.aliyun.com"); // ntp获取时间，你也可用其他"pool.ntp.org","0.cn.pool.ntp.org","1.cn.pool.ntp.org","ntp1.aliyun.com"
	delay(200);
}

void loop() {
	// delay(1);
	// Serial.printf("Free heap: %d bytes\n", EspClass::getFreeHeap());

	// 处理网络请求
	if (beginSettings.webConfig.enable) {
		server->handleClient();
		if (!configStatus) {
			return;
		}
	}

	if (first) { // 首次加载
		updateDataAll(&display);
		first = false;
	}
	if (millis() - timeSinceLastWUpdate > (1000L * UPDATE_INTERVAL_SECS)) { // 屏幕刷新
		setReadyForWeatherUpdate();
		timeSinceLastWUpdate = millis();
	}

	if (millis() - timeSinceLastSHT30Update > (1000L * UPDATE_SHT30_INTERVAL_SECS)) { // SHT30更新
		sht30.readData(sht30Data);
		timeSinceLastSHT30Update = millis();
	}

	if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) { // 天气更新
		updateData(&display);
	}

	int remainingTimeBudget = ui.update(); // 剩余时间预算
	if (remainingTimeBudget > 0) {
		// 你可以在这里工作如果你低于你的时间预算。
		delay(remainingTimeBudget);
	}
}

void setDefaultWebConfig() {
	beginSettings.webConfig.enable = true;
	beginSettings.webConfig.port = 8080;
}

bool loadConfig(BeginSettings &beginSettings) {
	if (!LittleFS.begin()) {
		Serial.println("LittleFS.begin() failed");
		return false;
	}

	if (!LittleFS.exists("/config.json")) {
		Serial.println("Config.json not found");
		LittleFS.end();
		setDefaultWebConfig();
		return false;
	}

	// 打开 JSON 文件
	File configFile = LittleFS.open("/config.json", "r");
	if (!configFile) {
		Serial.println("Failed to open config file for reading");
		setDefaultWebConfig();
		return false;
	}

	// 解析 JSON
	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, configFile);
	if (error) {
		Serial.print("Failed to parse JSON: ");
		Serial.println(error.c_str());
		configFile.close();
		return false;
	}

	auto wifiConfig = doc["wifi"].as<JsonObject>();
	beginSettings.wifiConfig.ssid = wifiConfig["ssid"].as<String>();
	beginSettings.wifiConfig.pass = wifiConfig["password"].as<String>();
	wifiConfig.clear();

	auto hefengConfig = doc["hefeng"].as<JsonObject>();
	beginSettings.heFengConfig.key = hefengConfig["key"].as<String>();
	beginSettings.heFengConfig.location = hefengConfig["location"].as<String>();
	hefengConfig.clear();

	auto webConfig = doc["web"].as<JsonObject>();
	beginSettings.webConfig.enable = webConfig["enable"] | true;
	beginSettings.webConfig.port = webConfig["port"] | 8080;
	webConfig.clear();

	doc.clear();
	configFile.close();
	LittleFS.end();

	return true;
}

bool saveConfig(const String &data) {
	if (!LittleFS.begin()) {
		Serial.println("LittleFS.begin() failed");
		return false;
	}

	// 打开 JSON 文件
	File configFile = LittleFS.open("/config.json", "w");
	if (!configFile) {
		LittleFS.end();
		Serial.println("Failed to open config file for reading");
		return false;
	}

	configFile.print(data);
	configFile.flush();

	configFile.close();
	LittleFS.end();

	return true;
}

String readConfig() {
	if (!LittleFS.begin()) {
		Serial.println("LittleFS.begin() failed");
		return R"("status": -1, "message": "LittleFS failed")";
	}

	File configFile = LittleFS.open("/config.json", "r");
	if (!configFile) {
		Serial.println("Failed to open config file for reading");
		LittleFS.end();
		return R"("status": -1, "message": "failed")";
	}

	String data = configFile.readString();

	configFile.close();
	LittleFS.end();
	return data;
}

bool wifiConnect() {
	display.clear();
	display.drawXbm(0, 0, 128, 64, bilibili); // 显示哔哩哔哩
	display.display();

	Serial.print("start Wifi...^_^");
	WiFi.mode(WIFI_STA);
	WiFi.setHostname("HY-ESP8266");
	WiFi.setAutoReconnect(true);
	IPAddress dns1(114, 114, 114, 114);
	WiFi.config(0, 0, 0, dns1);

	for (int i = 0; i < 3; i++) {
		WiFi.begin(beginSettings.wifiConfig.ssid, beginSettings.wifiConfig.pass);
		for (int j = 0; j < 3; j++) {
			Serial.print(".");
			delay(5000);
		}
		if (WiFi.status() == WL_CONNECTED) {
			Serial.println("success");
			break;
		}
	}

	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("failed");
		return false;
	}
	Serial.println("connected...^_^");
	return true;
}

void setMode() {
	WiFi.mode(WIFI_AP);
	WiFi.softAP("ConfigAP", "12345678"); // 启动 AP 模式
}

void webServer() {
	server = new ESP8266WebServer(beginSettings.webConfig.port);
	// 启动 Web 服务器
	server->on("/setConfig", HTTP_POST, []() {
		if (server->hasArg("plain")) {
			String json = server->arg("plain");
			if (saveConfig(json)) {
				server->send(200, "application/json", R"({"status":"success"})");
			} else {
				server->send(500, "application/json",
							R"({"status":"error","message":"Failed to save configuration."})");
			}
		} else {
			server->send(400, "application/json", R"({"status":"error","message":"Invalid JSON payload."})");
		}
	});

	server->on("/getConfig", HTTP_GET, []() {
		server->send(200, "application/json", readConfig());
	});

	server->on("/reboot", HTTP_POST, []() {
		server->send(200, "application/json", R"({"status":"success"})");
		delay(3000);
		EspClass::restart();
	});

	server->onNotFound([]() {
		server->send(404, "text/plain", "Not found");
	});
	server->begin();
}

void displayStatus(int16_t x, int16_t y,const String &msg) {
	display.setTextAlignment(TEXT_ALIGN_LEFT);
	display.setFont(ArialMT_Plain_10);
	display.drawString(x, y, msg);
	display.display();
}

void drawProgress(OLEDDisplay *display, int percentage, const String& label) { // 绘制进度
	display->clear();
	display->setTextAlignment(TEXT_ALIGN_CENTER);
	display->setFont(ArialMT_Plain_10);
	display->drawString(64, 10, label);
	display->drawProgressBar(2, 28, 124, 10, percentage);
	display->display();
}

void updateData(OLEDDisplay *display) { // 天气更新
	HeFeng::doUpdateCurr(&currentWeather, beginSettings.heFengConfig.key, beginSettings.heFengConfig.location);
	HeFeng::doUpdateFore(foreWeather, beginSettings.heFengConfig.key, beginSettings.heFengConfig.location);
	readyForWeatherUpdate = false;
}

void updateDataAll(OLEDDisplay *display) { // 首次天气更新
	drawProgress(display, 0, "Updating SHT30...");
	sht30.readData(sht30Data);

	drawProgress(display, 33, "Updating weather...");
	HeFeng::doUpdateCurr(&currentWeather, beginSettings.heFengConfig.key, beginSettings.heFengConfig.location);

	drawProgress(display, 66, "Updating forecasts...");
	HeFeng::doUpdateFore(foreWeather, beginSettings.heFengConfig.key, beginSettings.heFengConfig.location);

	readyForWeatherUpdate = false;
	drawProgress(display, 100, "Done...");
	delay(200);
}

int16_t add_draw(int arg1, int arg2) { return static_cast<int16_t>(arg1 + arg2); }

void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) { // 显示时间
	now = time(nullptr);
	struct tm *timeInfo = localtime(&now);
	char buff[16];

	display->setTextAlignment(TEXT_ALIGN_CENTER);
	display->setFont(ArialMT_Plain_16);
	String date = WDAY_NAMES[timeInfo->tm_wday];

	sprintf_P(buff, PSTR("%04d-%02d-%02d  %s"), timeInfo->tm_year + 1900, timeInfo->tm_mon + 1, timeInfo->tm_mday,
			  WDAY_NAMES[timeInfo->tm_wday].c_str());
	display->drawString(add_draw(64, x), add_draw(5, y), String(buff));
	display->setFont(ArialMT_Plain_24);

	sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
	display->drawString(add_draw(64, x), add_draw(22, y), String(buff));
	display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) { // 显示天气
	display->setFont(ArialMT_Plain_10);
	display->setTextAlignment(TEXT_ALIGN_CENTER);
	display->drawString(add_draw(64, x), add_draw(38, y),
						currentWeather.cond_txt + "    |   Wind: " + currentWeather.wind_sc + "  ");

	display->setFont(ArialMT_Plain_24);
	display->setTextAlignment(TEXT_ALIGN_LEFT);
	String temp = currentWeather.tmp + "°C";
	display->drawString(add_draw(60, x), add_draw(3, y), temp);
	display->setFont(ArialMT_Plain_10);
	display->drawString(add_draw(62, x), add_draw(26, y), currentWeather.fl + "°C | " + currentWeather.hum + "%");
	display->setFont(Meteocons_Plain_36);
	display->setTextAlignment(TEXT_ALIGN_CENTER);
	display->drawString(add_draw(32, x), add_draw(0, y), currentWeather.iconMeteoCon);
}

void drawForecast(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) { // 天气预报
	drawForecastDetails(display, x, y, 0);
	drawForecastDetails(display, x + 44, y, 1);
	drawForecastDetails(display, x + 88, y, 2);
}

void drawDeviceInfo(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y) {
	// 设备信息
	display->setTextAlignment(TEXT_ALIGN_CENTER);
	display->setFont(ArialMT_Plain_16);
	display->drawString(add_draw(64, x), add_draw(5, y), WiFi.hostname());
	display->drawString(add_draw(64, x), add_draw(22, y), WiFi.localIP().toString());
}

void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) { // 天气预报
	display->setTextAlignment(TEXT_ALIGN_CENTER);
	display->setFont(ArialMT_Plain_10);
	display->drawString(add_draw(x, 20), add_draw(y, 0), foreWeather[dayIndex].datestr);
	display->setFont(Meteocons_Plain_21);
	display->drawString(add_draw(x, 20), add_draw(y, 12), foreWeather[dayIndex].iconMeteoCon);

	String temp = foreWeather[dayIndex].tmp_min + " | " + foreWeather[dayIndex].tmp_max;
	display->setFont(ArialMT_Plain_10);
	display->drawString(add_draw(x, 20), add_draw(y, 34), temp);
	display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state) { // 绘图页眉覆盖
	now = time(nullptr);
	struct tm *timeInfo = localtime(&now);
	char buff[14];
	sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);
	display->setColor(WHITE);
	display->setFont(ArialMT_Plain_10);
	display->setTextAlignment(TEXT_ALIGN_LEFT);
	display->drawString(6, 54, String(buff));
	display->setTextAlignment(TEXT_ALIGN_RIGHT);
	sprintf_P(buff, PSTR("%.1f°C"), sht30Data.rawTemperature);
	display->drawString(122, 54, String(buff));
	display->drawHorizontalLine(0, 52, 128);
}

void drawHeaderOverlayTmp(OLEDDisplay *display, OLEDDisplayUiState *state) {
	char buff[14];
	display->setColor(WHITE);
	display->setFont(ArialMT_Plain_10);
	display->setTextAlignment(TEXT_ALIGN_LEFT);
	sprintf_P(buff, PSTR("%.1f°C"), sht30Data.rawTemperature);
	display->drawString(6, 54, String(buff)); // 温度
	display->setTextAlignment(TEXT_ALIGN_RIGHT);
	sprintf_P(buff, PSTR("%.1f%%"), sht30Data.rawHumidity);
	display->drawString(122, 54, String(buff)); //湿度
	display->drawHorizontalLine(0, 52, 128);
}

void setReadyForWeatherUpdate() {  //为天气更新做好准备
	Serial.println("Setting readyForUpdate to true");
	readyForWeatherUpdate = true;
}
