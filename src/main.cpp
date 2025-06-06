#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <ctime>
#include <sys/time.h>

#include <OLEDDisplayUi.h>
#include <SSD1306Wire.h> //0.96寸用这个

#include <Wire.h>
#include "sht32/SHT30.h"

#include <FS.h>
#include <LittleFS.h>

#include "Begin.h"
#include "font/WeatherStationFonts.h"
#include "font/WeatherStationImages.h"
#include "hefeng/HeFeng.h"
#include "web/WebManager.h"

#define TZ 8 // 中国时区为8
#define DST_MN 0 // 默认为0

constexpr int UPDATE_INTERVAL_SECS = 5 * 60; // 5分钟更新一次天气
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
HeFeng* hefeng;
SHT30Data sht30Data;
BeginSettings beginSettings;
WebManager* webManager;

#define TZ_MN ((TZ) * 60) // 时间换算
#define TZ_SEC ((TZ) * 3600)
#define DST_SEC ((DST_MN) * 60)

time_t now; // 实例化时间

bool readyForWeatherUpdate = false; // 天气更新标志
bool first = true; // 首次更新标志
bool status = true; // 配置文件状态
bool wifiConnected = true; // 网络连接状态
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
void wifi_event_handler(System_Event_t *event);
void WifiAP();
bool wifiConnect();
int16_t add_draw(int arg1, int arg2);
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

	display.clear();
	if (!LittleFS.begin()) {
		displayStatus(2, 1,"LittleFS.begin() failed ");
		Serial.println("LittleFS.begin() failed");
		status = false;
		return;
	}

	if (loadConfig(beginSettings)) {
		displayStatus(2, 1,"Config loaded OK.");
		Serial.println("Config loaded successfully.");
	} else {
		displayStatus(2, 1,"Config loaded Failed.");
		Serial.println("Config loaded Failed.");
		status = false;
		return;
	}

	webManager = new WebManager(&beginSettings.webConfig);
	webManager->defaultRoute();
	webManager->begin(beginSettings.wifiConfig.hostname);

	if (wifiConnect()) {
		displayStatus(6, 1,"Wifi Connected OK.");
	} else {
		WifiAP();
		display.clear();
		displayStatus(4, 1,"Wifi Connected Failed.");
		displayStatus(4, 10,"Dev IP: " + WiFi.softAPIP().toString());
		Serial.println("Wifi Connected Failed.");
		wifiConnected = false;
		return;
	}

	wifi_set_event_handler_cb(wifi_event_handler);

	hefeng = new HeFeng(&beginSettings.heFengConfig);

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

	// 基础配置文件加载失败
	if (!status) {
		return;
	}

	webManager->loop();

	// wifi 链接失败AP模式 跳过无用操作显示
	if (!wifiConnected) {
		return;
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

	if (const int remainingTimeBudget = ui.update(); remainingTimeBudget > 0) {
		// 你可以在这里工作如果你低于你的时间预算。
		delay(remainingTimeBudget);
	}
}

bool loadConfig(BeginSettings &beginSettings) {
	if (!LittleFS.exists("/config.json")) {
		Serial.println("Config.json not found");
		return false;
	}

	// 打开 JSON 文件
	File configFile = LittleFS.open("/config.json", "r");
	if (!configFile) {
		Serial.println("Failed to open config file for reading");
		return false;
	}

	// 解析 JSON
	JsonDocument doc;
	const DeserializationError error = deserializeJson(doc, configFile);
	if (error) {
		Serial.print("Failed to parse JSON: ");
		Serial.println(error.c_str());
		configFile.close();
		return false;
	}

	const auto wifiConfig = doc["wifi"].as<JsonObject>();
	beginSettings.wifiConfig.ssid = wifiConfig["ssid"].as<String>();
	beginSettings.wifiConfig.pass = wifiConfig["password"].as<String>();
	beginSettings.wifiConfig.hostname = wifiConfig["hostname"].as<String>();
	wifiConfig.clear();

	const auto hefengConfig = doc["hefeng"].as<JsonObject>();
	beginSettings.heFengConfig.baseUrl = hefengConfig["baseUrl"].as<String>();
	beginSettings.heFengConfig.key = hefengConfig["key"].as<String>();
	beginSettings.heFengConfig.location = hefengConfig["location"].as<String>();
	hefengConfig.clear();

	const auto webConfig = doc["web"].as<JsonObject>();
	beginSettings.webConfig.port = webConfig["port"] | 8080;
	beginSettings.webConfig.username = webConfig["username"].as<String>();
	beginSettings.webConfig.password = webConfig["password"].as<String>();
	webConfig.clear();

	doc.clear();
	configFile.close();

	return true;
}

void wifi_event_handler(System_Event_t *event) {
	if (event->event == EVENT_STAMODE_GOT_IP) {
		readyForWeatherUpdate = true;
	}
}

void WifiAP() {
	WiFi.mode(WIFI_AP);
	WiFi.setHostname(beginSettings.wifiConfig.hostname.c_str());
	WiFi.softAP(beginSettings.wifiConfig.hostname.c_str(), "12345678", 1);
}

bool wifiConnect() {
	display.clear();
	display.drawXbm(0, 0, 128, 64, bilibili); // 显示哔哩哔哩
	display.display();

	Serial.print("start Wifi...^_^");
	WiFi.mode(WIFI_STA);
	WiFi.setHostname(beginSettings.wifiConfig.hostname.c_str());
	WiFi.setAutoReconnect(true);
	const IPAddress dns1(114, 114, 114, 114);
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
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());
	Serial.print("SSID: ");
	Serial.println(WiFi.SSID());
	return true;
}


void displayStatus(const int16_t x, const int16_t y,const String &msg) {
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
	hefeng->doUpdateCurr(&currentWeather);
	hefeng->doUpdateFore(foreWeather);
	readyForWeatherUpdate = false;
}

void updateDataAll(OLEDDisplay *display) { // 首次天气更新
	drawProgress(display, 0, "Updating SHT30...");
	sht30.readData(sht30Data);

	drawProgress(display, 33, "Updating weather...");
	hefeng->doUpdateCurr(&currentWeather);

	drawProgress(display, 66, "Updating forecasts...");
	hefeng->doUpdateFore(foreWeather);

	readyForWeatherUpdate = false;
	drawProgress(display, 100, "Done...");
	delay(200);
}

int16_t add_draw(const int arg1, const int arg2) { return static_cast<int16_t>(arg1 + arg2); }

void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState *state, const int16_t x, const int16_t y) { // 显示时间
	now = time(nullptr);
	const tm *timeInfo = localtime(&now);
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

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState *state, const int16_t x, const int16_t y) { // 显示天气
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

void drawDeviceInfo(OLEDDisplay *display, OLEDDisplayUiState *state, const int16_t x, const int16_t y) {
	// 设备信息
	display->setTextAlignment(TEXT_ALIGN_CENTER);
	display->setFont(ArialMT_Plain_16);
	display->drawString(add_draw(64, x), add_draw(5, y), WiFi.hostname());
	display->drawString(add_draw(64, x), add_draw(22, y), WiFi.localIP().toString());
}

void drawForecastDetails(OLEDDisplay *display, const int x, const int y, const int dayIndex) { // 天气预报
	display->setTextAlignment(TEXT_ALIGN_CENTER);
	display->setFont(ArialMT_Plain_10);
	display->drawString(add_draw(x, 20), add_draw(y, 0), foreWeather[dayIndex].datestr);
	display->setFont(Meteocons_Plain_21);
	display->drawString(add_draw(x, 20), add_draw(y, 12), foreWeather[dayIndex].iconMeteoCon);

	const String temp = foreWeather[dayIndex].tmp_min + " | " + foreWeather[dayIndex].tmp_max;
	display->setFont(ArialMT_Plain_10);
	display->drawString(add_draw(x, 20), add_draw(y, 34), temp);
	display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState *state) { // 绘图页眉覆盖
	now = time(nullptr);
	const tm *timeInfo = localtime(&now);
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
