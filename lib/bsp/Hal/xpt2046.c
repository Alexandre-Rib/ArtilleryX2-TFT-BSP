#include "xpt2046.h"
#include "GPIO_Init.h"
#include "sw_spi.h"

static _SW_SPI xpt2046;
static uint32_t tpen_pin;

static void XPT2046_CS_Set(uint8_t level)
{
  SW_SPI_CS_Set(&xpt2046, level);
}

static uint8_t XPT2046_ReadWriteByte(uint8_t TxData)
{
  return SW_SPI_Read_Write(&xpt2046, TxData);
}

void XPT2046_Init(uint32_t tpen, uint32_t cs, uint32_t sck, uint32_t miso, uint32_t mosi)
{
  tpen_pin = tpen;

  GPIO_InitSet(tpen, MGPIO_MODE_IPN, 0);

  SW_SPI_Config(&xpt2046, _SPI_MODE3, 8, cs, sck, miso, mosi);

  XPT2046_CS_Set(1);
}

uint8_t XPT2046_Read_Pen(void)
{
  return GPIO_GetLevel(tpen_pin);
}

/******************************************************************************/

static uint16_t XPT2046_Read_AD(uint8_t CMD)
{
  uint16_t ADNum;

  XPT2046_CS_Set(0);

  XPT2046_ReadWriteByte(CMD);
  ADNum  = (XPT2046_ReadWriteByte(0xff) << 8);
  ADNum |=  XPT2046_ReadWriteByte(0xff);
  ADNum >>= 4;  // XPT2046 data is only 12 bits

  XPT2046_CS_Set(1);

  return ADNum;
}

#define READ_TIMES 5
#define LOST_VAL   1

static uint16_t XPT2046_Average_AD(uint8_t CMD)
{
  uint16_t i, j;
  uint16_t buf[READ_TIMES];
  uint16_t sum  = 0;
  uint16_t temp;

  for (i = 0; i < READ_TIMES; i++) buf[i] = XPT2046_Read_AD(CMD);

  for (i = 0; i < READ_TIMES - 1; i++)
    for (j = i + 1; j < READ_TIMES; j++)
      if (buf[i] > buf[j]) { temp = buf[i]; buf[i] = buf[j]; buf[j] = temp; }

  for (i = LOST_VAL; i < READ_TIMES - LOST_VAL; i++) sum += buf[i];

  return sum / (READ_TIMES - 2 * LOST_VAL);
}

#define ERR_RANGE 50

uint16_t XPT2046_Repeated_Compare_AD(uint8_t CMD)
{
  uint16_t ad1 = XPT2046_Average_AD(CMD);
  uint16_t ad2 = XPT2046_Average_AD(CMD);

  if ((ad2 <= ad1 && ad1 < ad2 + ERR_RANGE) || (ad1 <= ad2 && ad2 < ad1 + ERR_RANGE))
    return (ad1 + ad2) / 2;

  return 0;
}
