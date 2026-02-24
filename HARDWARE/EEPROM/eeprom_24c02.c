#include "eeprom_24c02.h"
#include <stddef.h> /* NULL */
#include "sys.h"
#include "stm32f4xx_conf.h"
#include "delay.h"

/* Board wiring: 24C02 on PB8/PB9 (bit-banged I2C, open-drain). */
#define EEPROM24C02_SCL_PORT GPIOB
#define EEPROM24C02_SCL_PIN  GPIO_Pin_8
#define EEPROM24C02_SDA_PORT GPIOB
#define EEPROM24C02_SDA_PIN  GPIO_Pin_9

/* 7-bit I2C address (A2..A0 = 0). */
#define EEPROM24C02_I2C_ADDR 0x50u

/* Typical write cycle time is a few ms; poll ACK up to this many times. */
#define EEPROM24C02_READY_RETRY 20u

static uint8_t g_eeprom24c02_inited = 0;

static void eeprom24c02_pin_write(GPIO_TypeDef *port, uint16_t pin, uint8_t high)
{
    if (high)
    {
        GPIO_SetBits(port, pin);
    }
    else
    {
        GPIO_ResetBits(port, pin);
    }
}

static uint8_t eeprom24c02_pin_read(GPIO_TypeDef *port, uint16_t pin)
{
    return (GPIO_ReadInputDataBit(port, pin) != Bit_RESET) ? 1u : 0u;
}

static void eeprom24c02_i2c_start(void)
{
    eeprom24c02_pin_write(EEPROM24C02_SDA_PORT, EEPROM24C02_SDA_PIN, 1);
    eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 1);
    delay_us(4);
    eeprom24c02_pin_write(EEPROM24C02_SDA_PORT, EEPROM24C02_SDA_PIN, 0);
    delay_us(4);
    eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 0);
}

static void eeprom24c02_i2c_stop(void)
{
    eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 0);
    eeprom24c02_pin_write(EEPROM24C02_SDA_PORT, EEPROM24C02_SDA_PIN, 0);
    delay_us(4);
    eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 1);
    delay_us(4);
    eeprom24c02_pin_write(EEPROM24C02_SDA_PORT, EEPROM24C02_SDA_PIN, 1);
    delay_us(2);
}

static void eeprom24c02_i2c_send_byte(uint8_t txd)
{
    uint8_t t;

    eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 0);
    for (t = 0; t < 8; t++)
    {
        eeprom24c02_pin_write(EEPROM24C02_SDA_PORT, EEPROM24C02_SDA_PIN, (txd & 0x80u) ? 1u : 0u);
        txd <<= 1;
        delay_us(2);
        eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 1);
        delay_us(2);
        eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 0);
        delay_us(2);
    }
}

static uint8_t eeprom24c02_i2c_wait_ack(void)
{
    uint16_t ucErrTime = 0;

    /* Release SDA (OD-high). */
    eeprom24c02_pin_write(EEPROM24C02_SDA_PORT, EEPROM24C02_SDA_PIN, 1);
    delay_us(1);
    eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 1);
    delay_us(1);

    while (eeprom24c02_pin_read(EEPROM24C02_SDA_PORT, EEPROM24C02_SDA_PIN))
    {
        ucErrTime++;
        if (ucErrTime > 250u)
        {
            eeprom24c02_i2c_stop();
            return 1;
        }
    }

    eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 0);
    return 0;
}

static void eeprom24c02_i2c_ack(void)
{
    eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 0);
    eeprom24c02_pin_write(EEPROM24C02_SDA_PORT, EEPROM24C02_SDA_PIN, 0);
    delay_us(2);
    eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 1);
    delay_us(2);
    eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 0);
    eeprom24c02_pin_write(EEPROM24C02_SDA_PORT, EEPROM24C02_SDA_PIN, 1);
}

static void eeprom24c02_i2c_nack(void)
{
    eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 0);
    eeprom24c02_pin_write(EEPROM24C02_SDA_PORT, EEPROM24C02_SDA_PIN, 1);
    delay_us(2);
    eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 1);
    delay_us(2);
    eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 0);
}

static uint8_t eeprom24c02_i2c_read_byte(uint8_t ack)
{
    uint8_t i;
    uint8_t receive = 0;

    /* Release SDA for input sampling. */
    eeprom24c02_pin_write(EEPROM24C02_SDA_PORT, EEPROM24C02_SDA_PIN, 1);
    for (i = 0; i < 8; i++)
    {
        eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 0);
        delay_us(4);
        eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 1);
        receive <<= 1;
        if (eeprom24c02_pin_read(EEPROM24C02_SDA_PORT, EEPROM24C02_SDA_PIN))
        {
            receive++;
        }
        delay_us(1);
    }

    if (ack)
    {
        eeprom24c02_i2c_ack();
    }
    else
    {
        eeprom24c02_i2c_nack();
    }

    return receive;
}

static uint8_t eeprom24c02_probe_addr(uint8_t addr_7bit)
{
    uint8_t ok;

    eeprom24c02_i2c_start();
    eeprom24c02_i2c_send_byte((uint8_t)((addr_7bit << 1) | 0u));
    ok = (eeprom24c02_i2c_wait_ack() == 0u) ? 1u : 0u;
    eeprom24c02_i2c_stop();

    return ok;
}

static void eeprom24c02_wait_ready(void)
{
    uint8_t retry = 0;

    while (retry < (uint8_t)EEPROM24C02_READY_RETRY)
    {
        if (eeprom24c02_probe_addr((uint8_t)EEPROM24C02_I2C_ADDR))
        {
            return;
        }
        delay_xms(1);
        retry++;
    }
}

void EEPROM24C02_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    if (g_eeprom24c02_inited)
    {
        return;
    }

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = EEPROM24C02_SCL_PIN | EEPROM24C02_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    eeprom24c02_pin_write(EEPROM24C02_SCL_PORT, EEPROM24C02_SCL_PIN, 1);
    eeprom24c02_pin_write(EEPROM24C02_SDA_PORT, EEPROM24C02_SDA_PIN, 1);
    delay_us(4);

    g_eeprom24c02_inited = 1;
}

uint8_t EEPROM24C02_WriteByte(uint8_t mem_addr, uint8_t data)
{
    EEPROM24C02_Init();

    eeprom24c02_i2c_start();
    eeprom24c02_i2c_send_byte((uint8_t)((EEPROM24C02_I2C_ADDR << 1) | 0u));
    if (eeprom24c02_i2c_wait_ack())
    {
        eeprom24c02_i2c_stop();
        return 1;
    }

    eeprom24c02_i2c_send_byte(mem_addr);
    if (eeprom24c02_i2c_wait_ack())
    {
        eeprom24c02_i2c_stop();
        return 2;
    }

    eeprom24c02_i2c_send_byte(data);
    if (eeprom24c02_i2c_wait_ack())
    {
        eeprom24c02_i2c_stop();
        return 3;
    }

    eeprom24c02_i2c_stop();
    eeprom24c02_wait_ready();
    return 0;
}

uint8_t EEPROM24C02_ReadByte(uint8_t mem_addr, uint8_t *out)
{
    uint8_t res;

    if (out == NULL)
    {
        return 1;
    }

    EEPROM24C02_Init();

    eeprom24c02_i2c_start();
    eeprom24c02_i2c_send_byte((uint8_t)((EEPROM24C02_I2C_ADDR << 1) | 0u));
    if (eeprom24c02_i2c_wait_ack())
    {
        eeprom24c02_i2c_stop();
        return 2;
    }

    eeprom24c02_i2c_send_byte(mem_addr);
    if (eeprom24c02_i2c_wait_ack())
    {
        eeprom24c02_i2c_stop();
        return 3;
    }

    eeprom24c02_i2c_start();
    eeprom24c02_i2c_send_byte((uint8_t)((EEPROM24C02_I2C_ADDR << 1) | 1u));
    if (eeprom24c02_i2c_wait_ack())
    {
        eeprom24c02_i2c_stop();
        return 4;
    }

    res = eeprom24c02_i2c_read_byte(0); /* NACK */
    eeprom24c02_i2c_stop();

    *out = res;
    return 0;
}

uint8_t EEPROM24C02_Read(uint8_t mem_addr, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    uint8_t err;

    if ((buf == NULL) || (len == 0))
    {
        return 1;
    }

    for (i = 0; i < len; i++)
    {
        err = EEPROM24C02_ReadByte((uint8_t)(mem_addr + i), &buf[i]);
        if (err)
        {
            return err;
        }
    }
    return 0;
}

uint8_t EEPROM24C02_Write(uint8_t mem_addr, const uint8_t *buf, uint8_t len)
{
    uint8_t i;
    uint8_t err;

    if ((buf == NULL) || (len == 0))
    {
        return 1;
    }

    for (i = 0; i < len; i++)
    {
        err = EEPROM24C02_WriteByte((uint8_t)(mem_addr + i), buf[i]);
        if (err)
        {
            return err;
        }
    }
    return 0;
}

