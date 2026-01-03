#ifndef __INA226_H
#define __INA226_H

#include "sys.h"
#include "myiic.h"
#include <stdint.h>

// INA226 I2C 7-bit address depends on A0/A1 strap.
// Common default (A0=A1=GND) is 0x40.
#define INA226_I2C_ADDR_DEFAULT 0x40

// Board schematic (SCH_Schematic_v2.1.pdf):
// - A0/A1 are tied to GND -> address is 0x40.
// - Shunt resistor R13 is 10mΩ (12VIN -> D12V).
#define INA226_RSHUNT_MOHM_DEFAULT 10

// INA226 register map
#define INA226_REG_CONFIG           0x00
#define INA226_REG_SHUNT_VOLTAGE    0x01
#define INA226_REG_BUS_VOLTAGE      0x02
#define INA226_REG_POWER            0x03
#define INA226_REG_CURRENT          0x04
#define INA226_REG_CALIBRATION      0x05
#define INA226_REG_MASK_ENABLE      0x06
#define INA226_REG_ALERT_LIMIT      0x07
#define INA226_REG_MANUFACTURER_ID  0xFE
#define INA226_REG_DIE_ID           0xFF

// INA226 LSBs (datasheet):
// - Shunt voltage LSB: 2.5 uV
// - Bus voltage   LSB: 1.25 mV
#define INA226_SHUNT_LSB_NUM_UV 5
#define INA226_SHUNT_LSB_DEN_UV 2
#define INA226_BUS_LSB_NUM_MV   5
#define INA226_BUS_LSB_DEN_MV   4

// Common configuration used by many INA226 examples:
// shunt+bus continuous conversion, 1.1ms conversion time, average=1.
#define INA226_CONFIG_DEFAULT 0x4127

uint8_t INA226_Init(uint8_t i2c_addr);
uint8_t INA226_Probe(uint8_t i2c_addr);

uint8_t INA226_WriteReg16(uint8_t i2c_addr, uint8_t reg, uint16_t value);
uint8_t INA226_ReadReg16(uint8_t i2c_addr, uint8_t reg, uint16_t *value);

uint8_t INA226_ReadManufacturerID(uint8_t i2c_addr, uint16_t *id);
uint8_t INA226_ReadDieID(uint8_t i2c_addr, uint16_t *id);

uint8_t INA226_ReadBusVoltage_mV(uint8_t i2c_addr, int32_t *bus_mV);
uint8_t INA226_ReadShuntVoltage_uV(uint8_t i2c_addr, int32_t *shunt_uV);

// rshunt_mohm: shunt resistor value (mΩ).
// 注意：只有当分流电阻串在“整个板卡供电回路”时，计算得到的电流/功耗才代表整机功耗；
// 若分流电阻仅监测某个支路，则读数仅代表该支路。
uint8_t INA226_ReadCurrent_mA(uint8_t i2c_addr, uint32_t rshunt_mohm, int32_t *current_mA);
uint8_t INA226_ReadPower_mW(uint8_t i2c_addr, uint32_t rshunt_mohm, int32_t *power_mW);

#endif
