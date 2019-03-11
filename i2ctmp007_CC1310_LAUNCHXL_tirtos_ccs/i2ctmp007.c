/*
 * Copyright (c) 2016-2017, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *    ======== i2ctmp007.c ========
 */
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/I2C.h>
#include <ti/display/Display.h>

/* Example/Board Header files */
#include "Board.h"

#define TASKSTACKSIZE       640

//#define TMP007_ADDR         0x40
#define SHT31_ADDR      0x44
#define SHT31_RE        0x2C  /* Die Temp Result Register */
#define SHT31_CLK_STR   0x06  /* Object Temp Result Register */

#ifndef Board_TMP_ADDR
#define Board_TMP_ADDR       SHT31_ADDR
#endif

static Display_Handle display;

typedef float           ft;

ft temp = 0;
ft hum = 0;
uint8_t status = 0;
uint8_t Celsius = 0;


typedef enum{
  NO_ERROR       = 0x00, // no error
  ACK_ERROR      = 0x01, // no acknowledgment error
  CHECKSUM_ERROR = 0x02, // checksum mismatch error
  TIMEOUT_ERROR  = 0x04, // timeout error
  PARM_ERROR     = 0x80, // parameter out of range error
}etError;




// Generator polynomial for CRC
#define POLYNOMIAL  0x131 // P(x) = x^8 + x^5 + x^4 + 1 = 100110001

//-----------------------------------------------------------------------------
static uint8_t SHT3X_CalcCrc(uint8_t data[], uint8_t nbrOfBytes)
{
  uint8_t bit;        // bit mask
  uint8_t crc = 0xFF; // calculated checksum
  uint8_t byteCtr;    // byte counter

  // calculates 8-Bit checksum with given polynomial
  for(byteCtr = 0; byteCtr < nbrOfBytes; byteCtr++)
  {
    crc ^= (data[byteCtr]);
    for(bit = 8; bit > 0; --bit)
    {
      if(crc & 0x80) crc = (crc << 1) ^ POLYNOMIAL;
      else           crc = (crc << 1);
    }
  }

  return crc;
}

//-----------------------------------------------------------------------------
static etError SHT3X_CheckCrc(uint8_t data[], uint8_t nbrOfBytes, uint8_t checksum)
{
  uint8_t crc;     // calculated checksum

  // calculates 8-Bit checksum
  crc = SHT3X_CalcCrc(data, nbrOfBytes);

  // verify checksum
  if(crc != checksum) return CHECKSUM_ERROR;
  else                return NO_ERROR;
}

//-----------------------------------------------------------------------------
static ft SHT3X_CalcTemperature(uint16_t rawValue)
{
  // calculate temperature [°C]
  // T = -45 + 175 * rawValue / (2^16-1)
  return 175.0f * (ft)rawValue / 65535.0f - 45.0f;
}

//-----------------------------------------------------------------------------
static ft SHT3X_CalcHumidity(uint16_t rawValue)
{
  // calculate relative humidity [%RH]
  // RH = rawValue / (2^16-1) * 100
  return 100.0f * (ft)rawValue / 65535.0f;
}


/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
//    unsigned int    i;
    uint16_t        temperature;
    uint16_t        humidity;
//    uint8_t         txBuffer[1];
//    uint8_t         rxBuffer[2];
    uint8_t         txBuffer[2];
    uint8_t         rxBuffer[6];
    I2C_Handle      i2c;
    I2C_Params      i2cParams;
    I2C_Transaction i2cTransaction;

    /* Call driver init functions */
    Display_init();
    GPIO_init();
    I2C_init();

    /* Configure the LED pin */
    GPIO_setConfig(Board_GPIO_LED0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    /* Open the HOST display for output */
    display = Display_open(Display_Type_UART, NULL);
    if (display == NULL) {
        while (1);
    }

    /* Turn on user LED */
    GPIO_write(Board_GPIO_LED0, Board_GPIO_LED_ON);
    Display_printf(display, 0, 0, "Starting the sht31 example\n");

    /* Create I2C for usage */
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
    if (i2c == NULL) {
        Display_printf(display, 0, 0, "Error Initializing I2C\n");
        while (1);
    }
    else {
        Display_printf(display, 0, 0, "I2C Initialized!\n");
    }

    while (1) {
        status = 0;

    /* Point to the T ambient register and read its 2 bytes */
    txBuffer[0] = SHT31_RE;
    txBuffer[1] = SHT31_CLK_STR;
    i2cTransaction.slaveAddress = SHT31_ADDR;
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 2;
    i2cTransaction.readBuf = rxBuffer;
    i2cTransaction.readCount = 0;

    if (I2C_transfer(i2c, &i2cTransaction)) {

        usleep(2000);

        Display_printf(display, 0, 0, "MSB: 0x%x  LSB: 0x%x\n", txBuffer[0], txBuffer[1]);
    }
    else {
        Display_printf(display, 0, 0, "I2C Bus fault\n");
    }

    i2cTransaction.writeCount = 0;
    i2cTransaction.readCount = 6;

    if (I2C_transfer(i2c, &i2cTransaction)) {

        if (CHECKSUM_ERROR == SHT3X_CheckCrc((uint8_t *)&rxBuffer[0], 2, rxBuffer[2])) {
            Display_printf(display, 0, 0, "Temperature CRC error\n");
        } else {
            temperature = ((rxBuffer[0] << 8) | (rxBuffer[1]));
            if (CHECKSUM_ERROR == SHT3X_CheckCrc((uint8_t *)&rxBuffer[3], 2, rxBuffer[5])) {
                        Display_printf(display, 0, 0, "Humidity CRC error\n");
            }
            humidity = ((rxBuffer[3] << 8) | (rxBuffer[4]));
        }
        status = 1;
    }
    else {
        Display_printf(display, 0, 0, "I2C Bus fault\n");
    }

    temp = SHT3X_CalcTemperature(temperature);
    hum = SHT3X_CalcHumidity(humidity);

    Celsius = (uint8_t)temp;

    Display_printf(display, 0, 0, "temperature is: %f(C)  humidity is: %f(%%RH)\n", temp, hum);

    /* Sleep for 1 second */
    sleep(1);

    }

    /* Deinitialized I2C */
//    I2C_close(i2c);
//    Display_printf(display, 0, 0, "I2C closed!\n");

//    return (NULL);
}
