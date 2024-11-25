//
// Created by 29154 on 2024/11/23.
//

#include "SHT30.h"

SHT30::SHT30(uint8_t i2cAddress, uint8_t sdaPin, uint8_t sclPin) {
	this->i2cAddress = i2cAddress;
	this->sdaPin = sdaPin;
	this->sclPin = sclPin;
}

void SHT30::begin() const { Wire.begin(sdaPin, sclPin); }

// CRC 校验函数
uint8_t SHT30::calculateCRC8(const uint8_t *data, int length) {
	uint8_t crc = 0xFF;
	for (int j = 0; j < length; j++) {
		crc ^= data[j];
		for (int i = 0; i < 8; i++) {
			constexpr uint8_t POLYNOMIAL = 0x31;
			if (crc & 0x80)
				crc = (crc << 1) ^ POLYNOMIAL;
			else
				crc = (crc << 1);
		}
	}
	return crc;
}

bool SHT30::readData(SHT30Data &s_data) const {
	// 发送读取命令
	Wire.beginTransmission(i2cAddress);
	Wire.write(0x2C); // 高重复性命令
	Wire.write(0x06);
	if (Wire.endTransmission() != 0) {
		return false; // 传输失败
	}

	// 等待数据处理完成
	delay(15);

	// 请求 6 字节数据
	Wire.requestFrom(i2cAddress, (uint8_t) 6);
	if (Wire.available() != 6) {
		return false; // 数据长度不正确
	}

	uint8_t data[6];
	for (unsigned char &i: data) {
		i = Wire.read();
	}

	// CRC 校验
	if (calculateCRC8(data, 2) != data[2] || calculateCRC8(data + 3, 2) != data[5]) {
		return false; // CRC 校验失败
	}

	// 计算温湿度
	int rawTemp = (data[0] << 8) | data[1];
	int rawHumidity = (data[3] << 8) | data[4];

	s_data.rawTemperature = static_cast<float>(-45.0 + 175.0 * (rawTemp / 65535.0));
	s_data.rawHumidity = static_cast<float>(100.0 * (rawHumidity / 65535.0));

    return true; // 数据读取成功
}