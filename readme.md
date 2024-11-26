# ESP8266-01S

> 通过ESP8266-01S模块实现显示天气信息和环境温度
> 
> 显示模块使用SSD1306
> 
> 温度传感器使用SHT30
> 
> 天气API使用和风天气
>

## 存在的问题
> getMeteoconIcon 函数存在问题获取的图标可能存在争议 请自行修改或者PR

## 接线表
| 模块         | 引脚    | 设备| 引脚 | 设备 | 引脚  |
|------------|-------| --- | --- | --- | --- |
| ESP8266-01S | VCC  | SSD1306 | VCC | SHT30 | VCC |
| ESP8266-01S | GND  | SSD1306 | GND | SHT30 | GND |
| ESP8266-01S | GPIO0 | SSD1306 | SDA | SHT30 | SDA |
| ESP8266-01S | GPIO2 | SSD1306 | SCL | SHT30 | SCL |

### 接线说明
接线图描述：
```
ESP8266-01S模块：
VCC：连接到电源的正极（3.3V）。
GND：连接到电源的负极。
GPIO0：连接到SSD1306模块的SDA引脚。
GPIO2：连接到SSD1306模块的SCL引脚。

SSD1306模块：
VCC：连接到电源的正极（与ESP8266-01S共享）。
GND：连接到电源的负极（与ESP8266-01S共享）。
SDA：连接到ESP8266-01S的GPIO0引脚。
SCL：连接到ESP8266-01S的GPIO2引脚。

SHT30传感器：
VCC：连接到电源的正极（与ESP8266-01S和SSD1306共享）。
GND：连接到电源的负极（与ESP8266-01S和SSD1306共享）。
SDA：连接到ESP8266-01S的GPIO0引脚(与SSD1306共用)。
SCL：连接到ESP8266-01S的GPIO2引脚(与SSD1306共用)。
```

## 配置文件
> 配置文件位于 `data/config.json` (自行新建文件)
> 
> 使用时请删除注解

```json
{
  "wifi": {
    "ssid": "WiFi 名称",
    "password": "WIFI 密码"
  },
  "hefeng": {
    "key": "和风天气API Key",
    "location": "和风天气API 位置/坐标"
  },
  "web": {
    "enable": true, // 是否启用Web服务(如果无法读取配置文件或wifi无法连接则该选项无效)
    "port": 8080 // Web服务端口 默认 8080
  }
}
```

## 编译
> 使用PlatformIO加载项目
> 
> 删除 `data` 目录下的 `.keep`
> 
> 下载依赖
> 
> Build Filesystem Image 打包LittleFS镜像
> 
> Upload 编译上传固件
> 
> Upload Filesystem Image 上传LittleFS镜像
> 

## API修改配置
> 发送POST请求到 `http://192.168.4.1:8080/setConfig` 参数为配置文件内容
> 
> 获取当前配置 `http://192.168.4.1:8080/getConfig`
> 
> 重启 `http://192.168.4.1:8080/reboot`
> 
> 连接到wifi只要web启动即可修改配置
>