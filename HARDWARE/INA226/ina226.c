#include "ina226.h"
#include <stddef.h>

static int32_t ina226_div_round_closest(int64_t num, int32_t den)
{
    if (den == 0)
    {
        return 0;
    }

    if (num >= 0)
    {
        return (int32_t)((num + (den / 2)) / den);
    }
    else
    {
        return (int32_t)((num - (den / 2)) / den);
    }
}

uint8_t INA226_WriteReg16(uint8_t i2c_addr, uint8_t reg, uint16_t value)
{
    IIC_Start();
    IIC_Send_Byte((i2c_addr << 1) | 0); // write
    if (IIC_Wait_Ack())
    {
        IIC_Stop();
        return 1;
    }

    IIC_Send_Byte(reg);
    if (IIC_Wait_Ack())
    {
        IIC_Stop();
        return 2;
    }

    IIC_Send_Byte((uint8_t)(value >> 8));
    if (IIC_Wait_Ack())
    {
        IIC_Stop();
        return 3;
    }

    IIC_Send_Byte((uint8_t)(value & 0xFF));
    if (IIC_Wait_Ack())
    {
        IIC_Stop();
        return 4;
    }

    IIC_Stop();
    return 0;
}

uint8_t INA226_ReadReg16(uint8_t i2c_addr, uint8_t reg, uint16_t *value)
{
    uint8_t msb = 0;
    uint8_t lsb = 0;

    if (value == NULL)
    {
        return 10;
    }

    IIC_Start();
    IIC_Send_Byte((i2c_addr << 1) | 0); // write
    if (IIC_Wait_Ack())
    {
        IIC_Stop();
        return 1;
    }

    IIC_Send_Byte(reg);
    if (IIC_Wait_Ack())
    {
        IIC_Stop();
        return 2;
    }

    IIC_Start();
    IIC_Send_Byte((i2c_addr << 1) | 1); // read
    if (IIC_Wait_Ack())
    {
        IIC_Stop();
        return 3;
    }

    msb = IIC_Read_Byte(1);
    lsb = IIC_Read_Byte(0);
    IIC_Stop();

    *value = ((uint16_t)msb << 8) | (uint16_t)lsb;
    return 0;
}

uint8_t INA226_Init(uint8_t i2c_addr)
{
    IIC_Init();
    return INA226_WriteReg16(i2c_addr, INA226_REG_CONFIG, INA226_CONFIG_DEFAULT);
}

uint8_t INA226_Probe(uint8_t i2c_addr)
{
    uint16_t manufacturer_id = 0;
    uint8_t err = INA226_ReadManufacturerID(i2c_addr, &manufacturer_id);
    if (err)
    {
        return err;
    }

    // TI manufacturer ID is typically 0x5449 ("TI").
    if (manufacturer_id != 0x5449)
    {
        return 100;
    }

    return 0;
}

uint8_t INA226_ReadManufacturerID(uint8_t i2c_addr, uint16_t *id)
{
    return INA226_ReadReg16(i2c_addr, INA226_REG_MANUFACTURER_ID, id);
}

uint8_t INA226_ReadDieID(uint8_t i2c_addr, uint16_t *id)
{
    return INA226_ReadReg16(i2c_addr, INA226_REG_DIE_ID, id);
}

uint8_t INA226_ReadBusVoltage_mV(uint8_t i2c_addr, int32_t *bus_mV)
{
    uint16_t raw = 0;
    uint8_t err = 0;

    if (bus_mV == NULL)
    {
        return 10;
    }

    err = INA226_ReadReg16(i2c_addr, INA226_REG_BUS_VOLTAGE, &raw);
    if (err)
    {
        return err;
    }

    // bus_mV = raw * 1.25mV = raw * 5 / 4
    *bus_mV = (int32_t)(((int64_t)raw * INA226_BUS_LSB_NUM_MV) / INA226_BUS_LSB_DEN_MV);
    return 0;
}

uint8_t INA226_ReadShuntVoltage_uV(uint8_t i2c_addr, int32_t *shunt_uV)
{
    uint16_t raw_u = 0;
    int16_t raw = 0;
    uint8_t err = 0;

    if (shunt_uV == NULL)
    {
        return 10;
    }

    err = INA226_ReadReg16(i2c_addr, INA226_REG_SHUNT_VOLTAGE, &raw_u);
    if (err)
    {
        return err;
    }

    raw = (int16_t)raw_u;
    // shunt_uV = raw * 2.5uV = raw * 5 / 2
    *shunt_uV = (int32_t)(((int64_t)raw * INA226_SHUNT_LSB_NUM_UV) / INA226_SHUNT_LSB_DEN_UV);
    return 0;
}

uint8_t INA226_ReadCurrent_mA(uint8_t i2c_addr, uint32_t rshunt_mohm, int32_t *current_mA)
{
    int32_t shunt_uV = 0;
    uint8_t err = 0;

    if (current_mA == NULL)
    {
        return 10;
    }
    if (rshunt_mohm == 0)
    {
        return 11;
    }

    err = INA226_ReadShuntVoltage_uV(i2c_addr, &shunt_uV);
    if (err)
    {
        return err;
    }

    // V(µV) = I(mA) * R(mΩ)  => I(mA) = V(µV) / R(mΩ)
    *current_mA = ina226_div_round_closest((int64_t)shunt_uV, (int32_t)rshunt_mohm);
    return 0;
}

uint8_t INA226_ReadPower_mW(uint8_t i2c_addr, uint32_t rshunt_mohm, int32_t *power_mW)
{
    int32_t bus_mV = 0;
    int32_t current_mA = 0;
    uint8_t err = 0;

    if (power_mW == NULL)
    {
        return 10;
    }

    err = INA226_ReadBusVoltage_mV(i2c_addr, &bus_mV);
    if (err)
    {
        return err;
    }

    err = INA226_ReadCurrent_mA(i2c_addr, rshunt_mohm, &current_mA);
    if (err)
    {
        return err;
    }

    // mV * mA = uW
    // mW = uW / 1000
    *power_mW = (int32_t)(((int64_t)bus_mV * (int64_t)current_mA) / 1000);
    return 0;
}
