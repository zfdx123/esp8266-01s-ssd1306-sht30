//
// Created by 29154 on 2024/11/23.
//

#ifndef SHT30_H
#define SHT30_H

#include <Wire.h>

typedef struct SHT30Data {
	float rawTemperature;
	float rawHumidity;
} SHT30Data;

class SHT30 {
private:
	uint8_t i2cAddress;
	uint8_t sdaPin, sclPin;
	static uint8_t calculateCRC8(const uint8_t *data, int length);

public:
	SHT30(uint8_t i2cAddress, uint8_t sdaPin, uint8_t sclPin);
	void begin() const;
	bool readData(SHT30Data &data) const;
};


#endif // SHT30_H
