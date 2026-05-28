#include "flash_map.h"
#include "w25qxx.h"  // W25Q64 driver — hidden from callers

void FlashMap_Read(uint32_t addr, uint8_t * buf, uint32_t size)
{
  W25Qxx_ReadBuffer(buf, addr, (uint16_t)size);
}

void FlashMap_Write(uint32_t addr, const uint8_t * buf, uint32_t size)
{
  W25Qxx_WriteBuffer((uint8_t *)buf, addr, (uint16_t)size);
}

void FlashMap_WritePage(uint32_t addr, const uint8_t * buf, uint16_t size)
{
  W25Qxx_WritePage((uint8_t *)buf, addr, size);
}

void FlashMap_EraseSector(uint32_t addr)
{
  W25Qxx_EraseSector(addr);
}
