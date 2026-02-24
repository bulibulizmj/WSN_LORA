#ifndef EEPROM_24C02_H
#define EEPROM_24C02_H

#include <stdint.h>

/* AT24C02 (2 Kbit) I2C EEPROM driver.
 * This project uses it to persist the NodeAddr across power loss.
 */

void EEPROM24C02_Init(void);

/* Return 0 on success, non-zero on error. */
uint8_t EEPROM24C02_ReadByte(uint8_t mem_addr, uint8_t *out);
uint8_t EEPROM24C02_WriteByte(uint8_t mem_addr, uint8_t data);

uint8_t EEPROM24C02_Read(uint8_t mem_addr, uint8_t *buf, uint8_t len);
uint8_t EEPROM24C02_Write(uint8_t mem_addr, const uint8_t *buf, uint8_t len);

#endif

