/***************************************************
  Arduino TFT graphics library targeted at ESP8266
  and ESP32 based boards.

  This is a standalone library that contains the
  hardware driver, the graphics functions and the
  proportional fonts.

  The larger fonts are Run Length Encoded to reduce their
  size.

  Created by Bodmer 2/12/16
  Bodmer: Added RPi 16 bit display support
 ****************************************************/


#include "In_eSPI.h"


        ////////////////////////////////////////////////////
        // TFT_eSPI driver functions for ESP32 processors //
        ////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
// Global variables
////////////////////////////////////////////////////////////////////////////////////////

// Select the SPI port to use, ESP32 has 2 options
#if !defined (TFT_PARALLEL_8_BIT)
  #ifdef USE_HSPI_PORT
    SPIClass spi = SPIClass(HSPI);
  #else // use default VSPI port
    SPIClass& spi = SPI;
  #endif
#endif

////////////////////////////////////////////////////////////////////////////////////////
#if defined (TFT_SDA_READ) && !defined (TFT_PARALLEL_8_BIT)
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           beginSDA
** Description:             Detach SPI from pin to permit software SPI
***************************************************************************************/
void TFT_eSPI::begin_SDA_Read(void)
{
  pinMatrixOutDetach(TFT_MOSI, false, false);
  pinMode(TFT_MOSI, INPUT);
  pinMatrixInAttach(TFT_MOSI, VSPIQ_IN_IDX, false);
}

/***************************************************************************************
** Function name:           endSDA
** Description:             Attach SPI pins after software SPI
***************************************************************************************/
void TFT_eSPI::end_SDA_Read(void)
{
  pinMode(TFT_MOSI, OUTPUT);
  pinMatrixOutAttach(TFT_MOSI, VSPID_OUT_IDX, false, false);
  pinMode(TFT_MISO, INPUT);
  pinMatrixInAttach(TFT_MISO, VSPIQ_IN_IDX, false);
}
////////////////////////////////////////////////////////////////////////////////////////
#endif // #if defined (TFT_SDA_READ)
////////////////////////////////////////////////////////////////////////////////////////


/***************************************************************************************
** Function name:           read byte  - supports class functions
** Description:             Read a byte from ESP32 8 bit data port
***************************************************************************************/
// Parallel bus MUST be set to input before calling this function!
uint8_t TFT_eSPI::readByte(void)
{
  uint8_t b = 0xAA;

#if defined (TFT_PARALLEL_8_BIT)
  RD_L;
  uint32_t reg;           // Read all GPIO pins 0-31
  reg = gpio_input_get(); // Read three times to allow for bus access time
  reg = gpio_input_get();
  reg = gpio_input_get(); // Data should be stable now
  RD_H;

  // Check GPIO bits used and build value
  b  = (((reg>>TFT_D0)&1) << 0);
  b |= (((reg>>TFT_D1)&1) << 1);
  b |= (((reg>>TFT_D2)&1) << 2);
  b |= (((reg>>TFT_D3)&1) << 3);
  b |= (((reg>>TFT_D4)&1) << 4);
  b |= (((reg>>TFT_D5)&1) << 5);
  b |= (((reg>>TFT_D6)&1) << 6);
  b |= (((reg>>TFT_D7)&1) << 7);
#endif

  return b;
}

////////////////////////////////////////////////////////////////////////////////////////
#ifdef TFT_PARALLEL_8_BIT
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           GPIO direction control  - supports class functions
** Description:             Set parallel bus to INPUT or OUTPUT
***************************************************************************************/
void TFT_eSPI::busDir(uint32_t mask, uint8_t mode)
{
  gpioMode(TFT_D0, mode);
  gpioMode(TFT_D1, mode);
  gpioMode(TFT_D2, mode);
  gpioMode(TFT_D3, mode);
  gpioMode(TFT_D4, mode);
  gpioMode(TFT_D5, mode);
  gpioMode(TFT_D6, mode);
  gpioMode(TFT_D7, mode);
  return;
  /*
  // Arduino generic native function, but slower
  pinMode(TFT_D0, mode);
  pinMode(TFT_D1, mode);
  pinMode(TFT_D2, mode);
  pinMode(TFT_D3, mode);
  pinMode(TFT_D4, mode);
  pinMode(TFT_D5, mode);
  pinMode(TFT_D6, mode);
  pinMode(TFT_D7, mode);
  return; //*/
}

/***************************************************************************************
** Function name:           GPIO direction control  - supports class functions
** Description:             Set ESP32 GPIO pin to input or output (set high) ASAP
***************************************************************************************/
void TFT_eSPI::gpioMode(uint8_t gpio, uint8_t mode)
{
  if(mode == INPUT) GPIO.enable_w1tc = ((uint32_t)1 << gpio);
  else GPIO.enable_w1ts = ((uint32_t)1 << gpio);

  ESP_REG(DR_REG_IO_MUX_BASE + esp32_gpioMux[gpio].reg) // Register lookup
    = ((uint32_t)2 << FUN_DRV_S)                        // Set drive strength 2
    | (FUN_IE)                                          // Input enable
    | ((uint32_t)2 << MCU_SEL_S);                       // Function select 2
  GPIO.pin[gpio].val = 1;                               // Set pin HIGH
}
////////////////////////////////////////////////////////////////////////////////////////
#endif // #ifdef TFT_PARALLEL_8_BIT
////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////
#if defined (RPI_WRITE_STROBE) && !defined (TFT_PARALLEL_8_BIT) // Code for RPi TFT
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           pushBlock - for ESP32 or ESP8266 RPi TFT
** Description:             Write a block of pixels of the same colour
***************************************************************************************/
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len)
{
  uint8_t colorBin[] = { (uint8_t) (color >> 8), (uint8_t) color };
  if(len) spi.writePattern(&colorBin[0], 2, 1); len--;
  while(len--) {WR_L; WR_H;}
}

/***************************************************************************************
** Function name:           pushPixels - for ESP32 or ESP8266 RPi TFT
** Description:             Write a sequence of pixels
***************************************************************************************/
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len)
{
  uint8_t *data = (uint8_t*)data_in;

  if(_swapBytes) {
      while ( len-- ) {tft_Write_16(*data); data++;}
      return;
  }

  while ( len >=64 ) {spi.writePattern(data, 64, 1); data += 64; len -= 64; }
  if (len) spi.writePattern(data, len, 1);
}

////////////////////////////////////////////////////////////////////////////////////////
#elif !defined (ILI9488_DRIVER) && !defined (TFT_PARALLEL_8_BIT) // Most displays
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           pushBlock - for ESP32
** Description:             Write a block of pixels of the same colour
***************************************************************************************/
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len){

  uint32_t color32 = (color<<8 | color >>8)<<16 | (color<<8 | color >>8);

  if (len > 31)
  {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), 511);
    while(len>31)
    {
      while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
      WRITE_PERI_REG(SPI_W0_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W1_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W2_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W3_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W4_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W5_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W6_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W7_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W8_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W9_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W10_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W11_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W12_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W13_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W14_REG(SPI_PORT), color32);
      WRITE_PERI_REG(SPI_W15_REG(SPI_PORT), color32);
      SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
      len -= 32;
    }
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
  }

  if (len)
  {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), (len << 4) - 1);
    for (uint32_t i=0; i <= (len<<1); i+=4) WRITE_PERI_REG(SPI_W0_REG(SPI_PORT) + i, color32);
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
  }
}

/***************************************************************************************
** Function name:           pushSwapBytePixels - for ESP32
** Description:             Write a sequence of pixels with swapped bytes
***************************************************************************************/
void TFT_eSPI::pushSwapBytePixels(const void* data_in, uint32_t len){

  uint8_t* data = (uint8_t*)data_in;
  uint32_t color[16];

  if (len > 31)
  {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), 511);
    while(len>31)
    {
      uint32_t i = 0;
      while(i<16)
      {
        color[i++] = DAT8TO32(data);
        data+=4;
      }
      while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
      WRITE_PERI_REG(SPI_W0_REG(SPI_PORT),  color[0]);
      WRITE_PERI_REG(SPI_W1_REG(SPI_PORT),  color[1]);
      WRITE_PERI_REG(SPI_W2_REG(SPI_PORT),  color[2]);
      WRITE_PERI_REG(SPI_W3_REG(SPI_PORT),  color[3]);
      WRITE_PERI_REG(SPI_W4_REG(SPI_PORT),  color[4]);
      WRITE_PERI_REG(SPI_W5_REG(SPI_PORT),  color[5]);
      WRITE_PERI_REG(SPI_W6_REG(SPI_PORT),  color[6]);
      WRITE_PERI_REG(SPI_W7_REG(SPI_PORT),  color[7]);
      WRITE_PERI_REG(SPI_W8_REG(SPI_PORT),  color[8]);
      WRITE_PERI_REG(SPI_W9_REG(SPI_PORT),  color[9]);
      WRITE_PERI_REG(SPI_W10_REG(SPI_PORT), color[10]);
      WRITE_PERI_REG(SPI_W11_REG(SPI_PORT), color[11]);
      WRITE_PERI_REG(SPI_W12_REG(SPI_PORT), color[12]);
      WRITE_PERI_REG(SPI_W13_REG(SPI_PORT), color[13]);
      WRITE_PERI_REG(SPI_W14_REG(SPI_PORT), color[14]);
      WRITE_PERI_REG(SPI_W15_REG(SPI_PORT), color[15]);
      SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
      len -= 32;
    }
  }

  if (len > 15)
  {
    uint32_t i = 0;
    while(i<8)
    {
      color[i++] = DAT8TO32(data);
      data+=4;
    }
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), 255);
    WRITE_PERI_REG(SPI_W0_REG(SPI_PORT),  color[0]);
    WRITE_PERI_REG(SPI_W1_REG(SPI_PORT),  color[1]);
    WRITE_PERI_REG(SPI_W2_REG(SPI_PORT),  color[2]);
    WRITE_PERI_REG(SPI_W3_REG(SPI_PORT),  color[3]);
    WRITE_PERI_REG(SPI_W4_REG(SPI_PORT),  color[4]);
    WRITE_PERI_REG(SPI_W5_REG(SPI_PORT),  color[5]);
    WRITE_PERI_REG(SPI_W6_REG(SPI_PORT),  color[6]);
    WRITE_PERI_REG(SPI_W7_REG(SPI_PORT),  color[7]);
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
    len -= 16;
  }

  if (len)
  {
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), (len << 4) - 1);
    for (uint32_t i=0; i <= (len<<1); i+=4) {
      WRITE_PERI_REG(SPI_W0_REG(SPI_PORT)+i, DAT8TO32(data)); data+=4;
    }
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
  }
  while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);

}

/***************************************************************************************
** Function name:           pushPixels - for ESP32
** Description:             Write a sequence of pixels
***************************************************************************************/
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len){

  if(_swapBytes) {
    pushSwapBytePixels(data_in, len);
    return;
  }

  uint32_t *data = (uint32_t*)data_in;

  if (len > 31)
  {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), 511);
    while(len>31)
    {
      while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
      WRITE_PERI_REG(SPI_W0_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W1_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W2_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W3_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W4_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W5_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W6_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W7_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W8_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W9_REG(SPI_PORT),  *data++);
      WRITE_PERI_REG(SPI_W10_REG(SPI_PORT), *data++);
      WRITE_PERI_REG(SPI_W11_REG(SPI_PORT), *data++);
      WRITE_PERI_REG(SPI_W12_REG(SPI_PORT), *data++);
      WRITE_PERI_REG(SPI_W13_REG(SPI_PORT), *data++);
      WRITE_PERI_REG(SPI_W14_REG(SPI_PORT), *data++);
      WRITE_PERI_REG(SPI_W15_REG(SPI_PORT), *data++);
      SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
      len -= 32;
    }
  }

  if (len)
  {
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), (len << 4) - 1);
    for (uint32_t i=0; i <= (len<<1); i+=4) WRITE_PERI_REG((SPI_W0_REG(SPI_PORT) + i), *data++);
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
  }
  while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
}

////////////////////////////////////////////////////////////////////////////////////////
#elif defined (ILI9488_DRIVER) && !defined (TFT_PARALLEL_8_BIT)// Now code for ILI9488
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           pushBlock - for ESP32 and 3 byte RGB display
** Description:             Write a block of pixels of the same colour
***************************************************************************************/
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len)
{
  // Split out the colours
  uint32_t r = (color & 0xF800)>>8;
  uint32_t g = (color & 0x07E0)<<5;
  uint32_t b = (color & 0x001F)<<19;
  // Concatenate 4 pixels into three 32 bit blocks
  uint32_t r0 = r<<24 | b | g | r;
  uint32_t r1 = r0>>8 | g<<16;
  uint32_t r2 = r1>>8 | b<<8;

  if (len > 19)
  {
    SET_PERI_REG_BITS(SPI_MOSI_DLEN_REG(SPI_PORT), SPI_USR_MOSI_DBITLEN, 479, SPI_USR_MOSI_DBITLEN_S);

    while(len>19)
    {
      while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
      WRITE_PERI_REG(SPI_W0_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W1_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W2_REG(SPI_PORT), r2);
      WRITE_PERI_REG(SPI_W3_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W4_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W5_REG(SPI_PORT), r2);
      WRITE_PERI_REG(SPI_W6_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W7_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W8_REG(SPI_PORT), r2);
      WRITE_PERI_REG(SPI_W9_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W10_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W11_REG(SPI_PORT), r2);
      WRITE_PERI_REG(SPI_W12_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W13_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W14_REG(SPI_PORT), r2);
      SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
      len -= 20;
    }
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
  }

  if (len)
  {
    SET_PERI_REG_BITS(SPI_MOSI_DLEN_REG(SPI_PORT), SPI_USR_MOSI_DBITLEN, (len * 24) - 1, SPI_USR_MOSI_DBITLEN_S);
    WRITE_PERI_REG(SPI_W0_REG(SPI_PORT), r0);
    WRITE_PERI_REG(SPI_W1_REG(SPI_PORT), r1);
    WRITE_PERI_REG(SPI_W2_REG(SPI_PORT), r2);
    WRITE_PERI_REG(SPI_W3_REG(SPI_PORT), r0);
    WRITE_PERI_REG(SPI_W4_REG(SPI_PORT), r1);
    WRITE_PERI_REG(SPI_W5_REG(SPI_PORT), r2);
    if (len > 8 )
    {
      WRITE_PERI_REG(SPI_W6_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W7_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W8_REG(SPI_PORT), r2);
      WRITE_PERI_REG(SPI_W9_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W10_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W11_REG(SPI_PORT), r2);
      WRITE_PERI_REG(SPI_W12_REG(SPI_PORT), r0);
      WRITE_PERI_REG(SPI_W13_REG(SPI_PORT), r1);
      WRITE_PERI_REG(SPI_W14_REG(SPI_PORT), r2);
    }

    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT))&SPI_USR);
  }
}

/***************************************************************************************
** Function name:           pushSwapBytePixels - for ESP32 and 3 byte RGB display
** Description:             Write a sequence of pixels with swapped bytes
***************************************************************************************/
void TFT_eSPI::pushSwapBytePixels(const void* data_in, uint32_t len){

  uint16_t *data = (uint16_t*)data_in;
  while ( len-- ) {tft_Write_16(*data); data++;}
}

/***************************************************************************************
** Function name:           pushPixels - for ESP32 and 3 byte RGB display
** Description:             Write a sequence of pixels
***************************************************************************************/
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len){

  uint16_t *data = (uint16_t*)data_in;
  if(_swapBytes) { while ( len-- ) {tft_Write_16(*data); data++;} }
  else { while ( len-- ) {tft_Write_16S(*data); data++;} }
}

////////////////////////////////////////////////////////////////////////////////////////
#elif defined (TFT_PARALLEL_8_BIT) // Now the code for ESP32 8 bit parallel
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           pushBlock - for ESP32 and parallel display
** Description:             Write a block of pixels of the same colour
***************************************************************************************/
void TFT_eSPI::pushBlock(uint16_t color, uint32_t len){
  if ( (color >> 8) == (color & 0x00FF) )
  { if (!len) return;
    tft_Write_16(color);
    while (--len) {WR_L; WR_H; WR_L; WR_H;}
  }
  else while (len--) {tft_Write_16(color);}
}

/***************************************************************************************
** Function name:           pushSwapBytePixels - for ESP32 and parallel display
** Description:             Write a sequence of pixels with swapped bytes
***************************************************************************************/
void TFT_eSPI::pushSwapBytePixels(const void* data_in, uint32_t len){

  uint16_t *data = (uint16_t*)data_in;
  while ( len-- ) {tft_Write_16(*data); data++;}
}

/***************************************************************************************
** Function name:           pushPixels - for ESP32 and parallel display
** Description:             Write a sequence of pixels
***************************************************************************************/
void TFT_eSPI::pushPixels(const void* data_in, uint32_t len){

  uint16_t *data = (uint16_t*)data_in;
  if(_swapBytes) { while ( len-- ) {tft_Write_16(*data); data++; } }
  else { while ( len-- ) {tft_Write_16S(*data); data++;} }
}

////////////////////////////////////////////////////////////////////////////////////////
#endif // End of display interface specific functions
////////////////////////////////////////////////////////////////////////////////////////



/***************************************************************************************
** Function name:           begin_tft_write (was called spi_begin)
** Description:             Start SPI transaction for writes and select TFT
***************************************************************************************/
inline void TFT_eSPI::begin_tft_write(void){
#if defined (SPI_HAS_TRANSACTION) && defined (SUPPORT_TRANSACTIONS) && !defined(ESP32_PARALLEL)
  if (locked) {
    locked = false;
    spi.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
    CS_L;
  }
#else
  CS_L;
#endif
  SET_BUS_WRITE_MODE;
}

/***************************************************************************************
** Function name:           end_tft_write (was called spi_end)
** Description:             End transaction for write and deselect TFT
***************************************************************************************/
inline void TFT_eSPI::end_tft_write(void){
#if defined (SPI_HAS_TRANSACTION) && defined (SUPPORT_TRANSACTIONS) && !defined(ESP32_PARALLEL)
  if(!inTransaction) {
    if (!locked) {
      locked = true;
      CS_H;
      spi.endTransaction();
    }
  }
  SET_BUS_READ_MODE;
#else
  if(!inTransaction) {CS_H;}
#endif
}

/***************************************************************************************
** Function name:           begin_tft_read  (was called spi_begin_read)
** Description:             Start transaction for reads and select TFT
***************************************************************************************/
// Reads require a lower SPI clock rate than writes
inline void TFT_eSPI::begin_tft_read(void){
  DMA_BUSY_CHECK; // Wait for any DMA transfer to complete before changing SPI settings
#if defined (SPI_HAS_TRANSACTION) && defined (SUPPORT_TRANSACTIONS) && !defined(ESP32_PARALLEL)
  if (locked) {
    locked = false;
    spi.beginTransaction(SPISettings(SPI_READ_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
    CS_L;
  }
#else
  #if !defined(ESP32_PARALLEL)
    spi.setFrequency(SPI_READ_FREQUENCY);
  #endif
   CS_L;
#endif
  SET_BUS_READ_MODE;
}

/***************************************************************************************
** Function name:           end_tft_read (was called spi_end_read)
** Description:             End transaction for reads and deselect TFT
***************************************************************************************/
inline void TFT_eSPI::end_tft_read(void){
#if defined (SPI_HAS_TRANSACTION) && defined (SUPPORT_TRANSACTIONS) && !defined(ESP32_PARALLEL)
  if(!inTransaction) {
    if (!locked) {
      locked = true;
      CS_H;
      spi.endTransaction();
    }
  }
#else
  #if !defined(ESP32_PARALLEL)
    spi.setFrequency(SPI_FREQUENCY);
  #endif
   if(!inTransaction) {CS_H;}
#endif
  SET_BUS_WRITE_MODE;
}

/***************************************************************************************
** Function name:           Legacy - deprecated
** Description:             Start/end transaction
***************************************************************************************/
  void TFT_eSPI::spi_begin()       {begin_tft_write();}
  void TFT_eSPI::spi_end()         {  end_tft_write();}
  void TFT_eSPI::spi_begin_read()  {begin_tft_read(); }
  void TFT_eSPI::spi_end_read()    {  end_tft_read(); }


/*
#ifndef ESP32_PARALLEL
  #include <SPI.h>
#endif


#if !defined (ESP32_PARALLEL)
  #ifdef USE_HSPI_PORT
    SPIClass spi = SPIClass(HSPI);
  #else // use default VSPI port
    SPIClass& spi = SPI;
  #endif
#endif


  // SUPPORT_TRANSACTIONS is mandatory for ESP32 so the hal mutex is toggled
#if defined (ESP32) && !defined (SUPPORT_TRANSACTIONS)
  #define SUPPORT_TRANSACTIONS
#endif

// If it is a 16bit serial display we must transfer 16 bits every time
#define CMD_BITS 8-1

// Fast block write prototype
void writeBlock(uint16_t color, uint32_t repeat);

// Byte read prototype
uint8_t readByte(void);

// GPIO parallel input/output control
void busDir(uint32_t mask, uint8_t mode);

void gpioMode(uint8_t gpio, uint8_t mode);

inline void TFT_eSPI::spi_begin(void){
#if defined (SPI_HAS_TRANSACTION) && defined (SUPPORT_TRANSACTIONS) && !defined(ESP32_PARALLEL)
  if (locked) {locked = false; spi.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE)); CS_L;}
#else
  CS_L;
#endif
}

inline void TFT_eSPI::spi_end(void){
#if defined (SPI_HAS_TRANSACTION) && defined (SUPPORT_TRANSACTIONS) && !defined(ESP32_PARALLEL)
  if(!inTransaction) {if (!locked) {locked = true; CS_H; spi.endTransaction();}}
#else
    if(!inTransaction) {CS_H;}
#endif
}

inline void TFT_eSPI::spi_begin_read(void){
#if defined (SPI_HAS_TRANSACTION) && defined (SUPPORT_TRANSACTIONS) && !defined(ESP32_PARALLEL)
  if (locked) {locked = false; spi.beginTransaction(SPISettings(SPI_READ_FREQUENCY, MSBFIRST, TFT_SPI_MODE)); CS_L;}
#else
  #if !defined(ESP32_PARALLEL)
    spi.setFrequency(SPI_READ_FREQUENCY);
  #endif
   CS_L;
#endif
}

inline void TFT_eSPI::spi_end_read(void){
#if defined (SPI_HAS_TRANSACTION) && defined (SUPPORT_TRANSACTIONS) && !defined(ESP32_PARALLEL)
  if(!inTransaction) {if (!locked) {locked = true; CS_H; spi.endTransaction();}}
#else
  #if !defined(ESP32_PARALLEL)
    spi.setFrequency(SPI_FREQUENCY);
  #endif
     if(!inTransaction) {CS_H;}
#endif
}

#if defined (TOUCH_CS) && defined (SPI_TOUCH_FREQUENCY) // && !defined(ESP32_PARALLEL)

  inline void TFT_eSPI::spi_begin_touch(void){
   CS_H; // Just in case it has been left low

  #if defined (SPI_HAS_TRANSACTION) && defined (SUPPORT_TRANSACTIONS)
    if (locked) {locked = false; spi.beginTransaction(SPISettings(SPI_TOUCH_FREQUENCY, MSBFIRST, SPI_MODE0));}
  #else
    spi.setFrequency(SPI_TOUCH_FREQUENCY);
  #endif

  T_CS_L;
  }

  inline void TFT_eSPI::spi_end_touch(void){
  T_CS_H;

  #if defined (SPI_HAS_TRANSACTION) && defined (SUPPORT_TRANSACTIONS)
    if(!inTransaction) {if (!locked) {locked = true; spi.endTransaction();}}
  #else
    spi.setFrequency(SPI_FREQUENCY);
  #endif

  }

#endif
*/


/***************************************************************************************
** Function name:           TFT_eSPI
** Description:             Constructor , we must use hardware SPI pins
***************************************************************************************/
TFT_eSPI::TFT_eSPI(int16_t w, int16_t h)
{

// The control pins are deliberately set to the inactive state (CS high) as setup()
// might call and initialise other SPI peripherals which would could cause conflicts
// if CS is floating or undefined.
#ifdef TFT_CS
  log_d("has TFT_CS: %d", TFT_CS);
  digitalWrite(TFT_CS, HIGH); // Chip select high (inactive)
  pinMode(TFT_CS, OUTPUT);
#endif

// Configure chip select for touchscreen controller if present
#ifdef TOUCH_CS
  log_d("has TOUCH_CS: %d", TOUCH_CS);
  digitalWrite(TOUCH_CS, HIGH); // Chip select high (inactive)
  pinMode(TOUCH_CS, OUTPUT);
#endif

#ifdef TFT_WR
  log_d("has TFT_WR: %d", TFT_WR);
  digitalWrite(TFT_WR, HIGH); // Set write strobe high (inactive)
  pinMode(TFT_WR, OUTPUT);
#endif

#ifdef TFT_DC
  log_d("has TFT_DC: %d", TFT_DC);
  digitalWrite(TFT_DC, HIGH); // Data/Command high = data mode
  pinMode(TFT_DC, OUTPUT);
#endif

#ifdef TFT_RST
  log_d("has TFT_RST: %d", TFT_RST);
  if (TFT_RST >= 0) {
    digitalWrite(TFT_RST, HIGH); // Set high, do not share pin with another SPI device
    pinMode(TFT_RST, OUTPUT);
  }
#endif

#ifdef ESP32_PARALLEL

  // Create a bit set lookup table for data bus - wastes 1kbyte of RAM but speeds things up dramatically
  for (int32_t c = 0; c<256; c++)
  {
    xset_mask[c] = 0;
    if ( c & 0x01 ) xset_mask[c] |= (1 << TFT_D0);
    if ( c & 0x02 ) xset_mask[c] |= (1 << TFT_D1);
    if ( c & 0x04 ) xset_mask[c] |= (1 << TFT_D2);
    if ( c & 0x08 ) xset_mask[c] |= (1 << TFT_D3);
    if ( c & 0x10 ) xset_mask[c] |= (1 << TFT_D4);
    if ( c & 0x20 ) xset_mask[c] |= (1 << TFT_D5);
    if ( c & 0x40 ) xset_mask[c] |= (1 << TFT_D6);
    if ( c & 0x80 ) xset_mask[c] |= (1 << TFT_D7);
  }

  // Make sure read is high before we set the bus to output
  digitalWrite(TFT_RD, HIGH);
  pinMode(TFT_RD, OUTPUT);

  GPIO.out_w1ts = set_mask(255); // Set data bus to 0xFF

  // Set TFT data bus lines to output
  busDir(dir_mask, OUTPUT);

#endif

  _init_width  = _width  = w; // Set by specific xxxxx_Defines.h file or by users sketch
  _init_height = _height = h; // Set by specific xxxxx_Defines.h file or by users sketch
  rotation  = 0;
  cursor_y  = cursor_x  = 0;
  textfont  = 1;
  textsize  = 1;
  textcolor   = bitmap_fg = 0xFFFF; // White
  textbgcolor = bitmap_bg = 0x0000; // Black
  padX = 0;             // No padding
  isDigits   = false;   // No bounding box adjustment
  textwrapX  = true;    // Wrap text at end of line when using print stream
  textwrapY  = false;   // Wrap text at bottom of screen when using print stream
  textdatum = TL_DATUM; // Top Left text alignment is default
  fontsloaded = 0;

  _swapBytes = false;   // Do not swap colour bytes by default

  locked = true;        // ESP32 transaction mutex lock flags
  inTransaction = false;

  _booted   = true;
  _cp437    = true;
  _utf8     = true;

#ifdef FONT_FS_AVAILABLE
  fs_font  = true;     // Smooth font filing system or array (fs_font = false) flag
#endif

#if defined (ESP32) && defined (CONFIG_SPIRAM_SUPPORT)
  if (psramFound()) _usePsram = true; // Enable the use of PSRAM (if available)
  else
#endif
  _usePsram = false;

  addr_row = 0xFFFF;
  addr_col = 0xFFFF;

  _xpivot = 0;
  _ypivot = 0;

  cspinmask = 0;
  dcpinmask = 0;
  wrpinmask = 0;
  sclkpinmask = 0;

#ifdef LOAD_GLCD
  fontsloaded  = 0x0002; // Bit 1 set
#endif

#ifdef LOAD_FONT2
  fontsloaded |= 0x0004; // Bit 2 set
#endif

#ifdef LOAD_FONT4
  fontsloaded |= 0x0010; // Bit 4 set
#endif

#ifdef LOAD_FONT6
  fontsloaded |= 0x0040; // Bit 6 set
#endif

#ifdef LOAD_FONT7
  fontsloaded |= 0x0080; // Bit 7 set
#endif

#ifdef LOAD_FONT8
  fontsloaded |= 0x0100; // Bit 8 set
#endif

#ifdef LOAD_FONT8N
  fontsloaded |= 0x0200; // Bit 9 set
#endif

#ifdef SMOOTH_FONT
  fontsloaded |= 0x8000; // Bit 15 set
#endif
}


/***************************************************************************************
** Function name:           begin
** Description:             Included for backwards compatibility
***************************************************************************************/
void TFT_eSPI::begin(uint8_t tc)
{
 init(tc);
}


/***************************************************************************************
** Function name:           init (tc is tab colour for ST7735 displays only)
** Description:             Reset, then initialise the TFT display registers
***************************************************************************************/
void TFT_eSPI::init(uint8_t tc)
{
  if (_booted)
  {

#if !defined(ESP32_PARALLEL)
  #if defined (TFT_MOSI) && !defined (TFT_SPI_OVERLAP)
    spi.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, -1);
  #else
    spi.begin();
  #endif
#endif

    inTransaction = false;
    locked = true;

#if defined(ESP32_PARALLEL)
    digitalWrite(TFT_CS, LOW); // Chip select low permanently
    pinMode(TFT_CS, OUTPUT);
#else
  #ifdef TFT_CS
    // Set to output once again in case D6 (MISO) is used for CS
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH); // Chip select high (inactive)
  #else
    spi.setHwCs(1); // Use hardware SS toggling
  #endif
#endif

  // Set to output once again in case D6 (MISO) is used for DC
#ifdef TFT_DC
    pinMode(TFT_DC, OUTPUT);
    digitalWrite(TFT_DC, HIGH); // Data/Command high = data mode
#endif

    _booted = false;
    end_tft_write();
  } // end of: if just _booted

  // Toggle RST low to reset
  begin_tft_write();

  bool lcd_version = 0;
#ifdef TFT_RST
  // M5Stack specific, try to detect IPS Lcd, will be used in init()
  pinMode(TFT_RST, INPUT_PULLDOWN);
  delay(1);
  if(digitalRead(TFT_RST)) {
    log_d("M5Stack Display is IPS");
    lcd_version = 1;
  }
  pinMode(TFT_RST, OUTPUT);
  if (TFT_RST >= 0) {
    digitalWrite(TFT_RST, HIGH);
    delay(5);
    digitalWrite(TFT_RST, LOW);
    delay(20);
    digitalWrite(TFT_RST, HIGH);
  }
  else writecommand(TFT_SWRST); // Software reset
#else
  writecommand(TFT_SWRST); // Software reset
#endif

  end_tft_write();

  delay(150); // Wait for reset to complete

  begin_tft_write();

  tc = tc; // Supress warning

  // This loads the driver specific initialisation code  <<<<<<<<<<<<<<<<<<<<< ADD NEW DRIVERS TO THE LIST HERE <<<<<<<<<<<<<<<<<<<<<<<
#if   defined (ILI9341_DRIVER)
    #include "TFT_Drivers/ILI9341_Init.h"

#elif defined (ST7735_DRIVER)
    tabcolor = tc;
    #include "TFT_Drivers/ST7735_Init.h"

#elif defined (ILI9163_DRIVER)
    #include "TFT_Drivers/ILI9163_Init.h"

#elif defined (S6D02A1_DRIVER)
    #include "TFT_Drivers/S6D02A1_Init.h"

#elif defined (ILI9486_DRIVER)
    #include "TFT_Drivers/ILI9486_Init.h"

#elif defined (ILI9481_DRIVER)
    #include "TFT_Drivers/ILI9481_Init.h"

#elif defined (ILI9488_DRIVER)
    #include "TFT_Drivers/ILI9488_Init.h"

#elif defined (HX8357D_DRIVER)
    #include "TFT_Drivers/HX8357D_Init.h"

#elif defined (ST7789_DRIVER)
    #include "TFT_Drivers/ST7789_Init.h"

#elif defined (R61581_DRIVER)
    #include "TFT_Drivers/R61581_Init.h"

#elif defined (ST7789_2_DRIVER)
    #include "TFT_Drivers/ST7789_2_Init.h"

#elif defined (WROVER_KIT_LCD_DRIVER)
    #include "TFT_Drivers/ST7789_WROVER_KIT_LCD_Init.h"

#elif defined (DDUINO32_XS_LCD_DRIVER)
    #include "TFT_Drivers/ST7789_DDUINO32-XS_Init.h"

#endif

#ifdef TFT_INVERSION_ON
  writecommand(TFT_INVON);
#endif

#ifdef TFT_INVERSION_OFF
  writecommand(TFT_INVOFF);
#endif

  end_tft_write();

  setRotation(rotation);

#if defined (TFT_BL) && defined (TFT_BACKLIGHT_ON)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#else
  #if defined (TFT_BL) && defined (M5STACK)
    // Turn on the back-light LED
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
  #endif
#endif
}


/***************************************************************************************
** Function name:           setRotation
** Description:             rotate the screen orientation m = 0-3 or 4-7 for BMP drawing
***************************************************************************************/
void TFT_eSPI::setRotation(uint8_t m)
{

  begin_tft_write();

    // This loads the driver specific rotation code  <<<<<<<<<<<<<<<<<<<<< ADD NEW DRIVERS TO THE LIST HERE <<<<<<<<<<<<<<<<<<<<<<<
#if   defined (ILI9341_DRIVER)
    #include "TFT_Drivers/ILI9341_Rotation.h"

#elif defined (ST7735_DRIVER)
    #include "TFT_Drivers/ST7735_Rotation.h"

#elif defined (ILI9163_DRIVER)
    #include "TFT_Drivers/ILI9163_Rotation.h"

#elif defined (S6D02A1_DRIVER)
    #include "TFT_Drivers/S6D02A1_Rotation.h"

#elif defined (ILI9486_DRIVER)
    #include "TFT_Drivers/ILI9486_Rotation.h"

#elif defined (ILI9481_DRIVER)
    #include "TFT_Drivers/ILI9481_Rotation.h"

#elif defined (ILI9488_DRIVER)
    #include "TFT_Drivers/ILI9488_Rotation.h"

#elif defined (HX8357D_DRIVER)
    #include "TFT_Drivers/HX8357D_Rotation.h"

#elif defined (ST7789_DRIVER)
    #include "TFT_Drivers/ST7789_Rotation.h"

#elif defined (R61581_DRIVER)
    #include "TFT_Drivers/R61581_Rotation.h"

#elif defined (ST7789_2_DRIVER)
    #include "TFT_Drivers/ST7789_2_Rotation.h"

#elif defined (WROVER_KIT_LCD_DRIVER)
    #include "TFT_Drivers/ST7789_WROVER_KIT_LCD_Rotation.h"

#elif defined(DDUINO32_XS_LCD_DRIVER)
    #include "TFT_Drivers/ST7789_DDUINO32-XS_Rotation.h"

#endif

  delayMicroseconds(10);

  end_tft_write();

  addr_row = 0xFFFF;
  addr_col = 0xFFFF;
}


/***************************************************************************************
** Function name:           commandList, used for FLASH based lists only (e.g. ST7735)
** Description:             Get initialisation commands from FLASH and send to TFT
***************************************************************************************/
void TFT_eSPI::commandList (const uint8_t *addr)
{
  uint8_t  numCommands;
  uint8_t  numArgs;
  uint8_t  ms;

  numCommands = pgm_read_byte(addr++);   // Number of commands to follow

  while (numCommands--)                  // For each command...
  {
    writecommand(pgm_read_byte(addr++)); // Read, issue command
    numArgs = pgm_read_byte(addr++);     // Number of args to follow
    ms = numArgs & TFT_INIT_DELAY;       // If hibit set, delay follows args
    numArgs &= ~TFT_INIT_DELAY;          // Mask out delay bit

    while (numArgs--)                    // For each argument...
    {
      writedata(pgm_read_byte(addr++));  // Read, issue argument
    }

    if (ms)
    {
      ms = pgm_read_byte(addr++);        // Read post-command delay time (ms)
      delay( (ms==255 ? 500 : ms) );
    }
  }

}


/***************************************************************************************
** Function name:           spiwrite
** Description:             Write 8 bits to SPI port (legacy support only)
***************************************************************************************/
void TFT_eSPI::spiwrite(uint8_t c)
{
  tft_Write_8(c);
}


/***************************************************************************************
** Function name:           writecommand
** Description:             Send an 8 bit command to the TFT
***************************************************************************************/
void TFT_eSPI::writecommand(uint8_t c)
{
  begin_tft_write(); // CS_L;

  DC_C;

  tft_Write_8(c);

  DC_D;

  end_tft_write();  // CS_H;

}


/***************************************************************************************
** Function name:           writedata
** Description:             Send a 8 bit data value to the TFT
***************************************************************************************/
void TFT_eSPI::writedata(uint8_t d)
{
  begin_tft_write(); // CS_L;

  DC_D;        // Play safe, but should already be in data mode

  tft_Write_8(d);

  CS_L;        // Allow more hold time for low VDI rail

  end_tft_write();   // CS_H;
}


/***************************************************************************************
** Function name:           readcommand8
** Description:             Read a 8 bit data value from an indexed command register
***************************************************************************************/
uint8_t TFT_eSPI::readcommand8(uint8_t cmd_function, uint8_t index)
{
  uint8_t reg = 0;
#ifdef ESP32_PARALLEL

  writecommand(cmd_function); // Sets DC and CS high

  busDir(dir_mask, INPUT);

  CS_L;

  // Read nth parameter (assumes caller discards 1st parameter or points index to 2nd)
  while(index--) reg = readByte();

  busDir(dir_mask, OUTPUT);

  CS_H;

#else
  // for ILI9341 Interface II i.e. IM [3:0] = "1101"
  begin_tft_read();
  index = 0x10 + (index & 0x0F);

  DC_C;  tft_Write_8(0xD9);
  DC_D;  tft_Write_8(index);

  CS_H; // Some displays seem to need CS to be pulsed here, or is just a delay needed?
  CS_L;

  DC_C;  tft_Write_8(cmd_function);
  DC_D;
  reg = tft_Read_8();

  end_tft_read();
#endif
  return reg;
}


/***************************************************************************************
** Function name:           readcommand16
** Description:             Read a 16 bit data value from an indexed command register
***************************************************************************************/
uint16_t TFT_eSPI::readcommand16(uint8_t cmd_function, uint8_t index)
{
  uint32_t reg;

  reg  = (readcommand8(cmd_function, index + 0) <<  8);
  reg |= (readcommand8(cmd_function, index + 1) <<  0);

  return reg;
}


/***************************************************************************************
** Function name:           readcommand32
** Description:             Read a 32 bit data value from an indexed command register
***************************************************************************************/
uint32_t TFT_eSPI::readcommand32(uint8_t cmd_function, uint8_t index)
{
  uint32_t reg;

  reg  = (readcommand8(cmd_function, index + 0) << 24);
  reg |= (readcommand8(cmd_function, index + 1) << 16);
  reg |= (readcommand8(cmd_function, index + 2) <<  8);
  reg |= (readcommand8(cmd_function, index + 3) <<  0);

  return reg;
}


/***************************************************************************************
** Function name:           read pixel (for SPI Interface II i.e. IM [3:0] = "1101")
** Description:             Read 565 pixel colours from a pixel
***************************************************************************************/
uint16_t TFT_eSPI::readPixel(int32_t x0, int32_t y0)
{
#if defined(ESP32_PARALLEL)

  readAddrWindow(x0, y0, 1, 1); // Sets CS low

  // Set masked pins D0- D7 to input
  busDir(dir_mask, INPUT);

  // Dummy read to throw away don't care value
  readByte();

  // Fetch the 16 bit BRG pixel
  //uint16_t rgb = (readByte() << 8) | readByte();

  #if defined (ILI9341_DRIVER) | defined (ILI9488_DRIVER) // Read 3 bytes

  // Read window pixel 24 bit RGB values and fill in LS bits
    uint16_t rgb = ((readByte() & 0xF8) << 8) | ((readByte() & 0xFC) << 3) | (readByte() >> 3);

    CS_H;

    // Set masked pins D0- D7 to output
    busDir(dir_mask, OUTPUT);

    return rgb;

    #else // ILI9481 16 bit read

    // Fetch the 16 bit BRG pixel
    uint16_t bgr = (readByte() << 8) | readByte();

    CS_H;

    // Set masked pins D0- D7 to output
    busDir(dir_mask, OUTPUT);

    // Swap Red and Blue (could check MADCTL setting to see if this is needed)
    //return  (bgr>>11) | (bgr<<11) | (bgr & 0x7E0);
    #ifdef ILI9486_DRIVER
      return  bgr;
    #else
      // Swap Red and Blue (could check MADCTL setting to see if this is needed)
      return  (bgr>>11) | (bgr<<11) | (bgr & 0x7E0);
    #endif
  #endif

#else // Not ESP32_PARALLEL

  bool wasInTransaction = inTransaction;
  if (inTransaction) { inTransaction= false; end_tft_write();}

  begin_tft_read();

  readAddrWindow(x0, y0, 1, 1); // Sets CS low

  #ifdef TFT_SDA_READ
    begin_SDA_Read();
  #endif

  // Dummy read to throw away don't care value
  tft_Read_8();
  #if defined (WROVER_KIT_LCD_DRIVER)
    tft_Read_8(); // Extra dummy read for WROVER_KIT_LCD_DRIVER (and possibly other ST7789)
  #endif

  //#if !defined (ILI9488_DRIVER)

    // Read the 3 RGB bytes, colour is actually only in the top 6 bits of each byte
    // as the TFT stores colours as 18 bits
    uint8_t r = tft_Read_8();
    uint8_t g = tft_Read_8();
    uint8_t b = tft_Read_8();
/*
  #else

    // The 6 colour bits are in MS 6 bits of each byte, but the ILI9488 needs an extra clock pulse
    // so bits appear shifted right 1 bit, so mask the middle 6 bits then shift 1 place left
    uint8_t r = (tft_Read_8()&0x7E)<<1;
    uint8_t g = (tft_Read_8()&0x7E)<<1;
    uint8_t b = (tft_Read_8()&0x7E)<<1;

  #endif
*/
  CS_H;

  #ifdef TFT_SDA_READ
    end_SDA_Read();
  #endif

  end_tft_read();

  if(wasInTransaction) { begin_tft_write(); inTransaction = true; }

  return color565(r, g, b);

#endif
}


void TFT_eSPI::setCallback(getColorCallback getCol)
{
  getColor = getCol;
}



/***************************************************************************************
** Function name:           read rectangle (for SPI Interface II i.e. IM [3:0] = "1101")
** Description:             Read 565 pixel colours from a defined area
***************************************************************************************/
void TFT_eSPI::readRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data)
{
  if ((x > _width) || (y > _height) || (w == 0) || (h == 0)) return;

#if defined(ESP32_PARALLEL)

  CS_L;

  readAddrWindow(x, y, w, h); // Sets CS low

  // Set masked pins D0- D7 to input
  busDir(dir_mask, INPUT);

  // Dummy read to throw away don't care value
  readByte();

  // Total pixel count
  uint32_t len = w * h;

#if defined (ILI9341_DRIVER) | defined (ILI9488_DRIVER) // Read 3 bytes
  // Fetch the 24 bit RGB value
  while (len--) {
    // Assemble the RGB 16 bit colour
    uint16_t rgb = ((readByte() & 0xF8) << 8) | ((readByte() & 0xFC) << 3) | (readByte() >> 3);

    // Swapped byte order for compatibility with pushRect()
    *data++ = (rgb<<8) | (rgb>>8);
  }
#else // ILI9481 reads as 16 bits
  // Fetch the 16 bit BRG pixels
  while (len--) {
    #ifdef ILI9486_DRIVER
      // Read the RGB 16 bit colour
      *data++ = readByte() | (readByte() << 8);
    #else
      // Read the BRG 16 bit colour
      uint16_t bgr = (readByte() << 8) | readByte();
      // Swap Red and Blue (could check MADCTL setting to see if this is needed)
      uint16_t rgb = (bgr>>11) | (bgr<<11) | (bgr & 0x7E0);
      // Swapped byte order for compatibility with pushRect()
      *data++ = (rgb<<8) | (rgb>>8);
    #endif
  }
#endif
  CS_H;

  // Set masked pins D0- D7 to output
  busDir(dir_mask, OUTPUT);

#else // Not ESP32_PARALLEL

  begin_tft_read();

  readAddrWindow(x, y, w, h); // Sets CS low

  #ifdef TFT_SDA_READ
    begin_SDA_Read();
  #endif

  // Dummy read to throw away don't care value
  tft_Read_8();
  #if defined (WROVER_KIT_LCD_DRIVER)
    tft_Read_8(); // Extra dummy read for WROVER_KIT_LCD_DRIVER (and possibly other ST7789)
  #endif

  // Read window pixel 24 bit RGB values
  uint32_t len = w * h;
  while (len--) {

  #if !defined (ILI9488_DRIVER)

    // Read the 3 RGB bytes, colour is actually only in the top 6 bits of each byte
    // as the TFT stores colours as 18 bits
    uint8_t r = tft_Read_8();
    uint8_t g = tft_Read_8();
    uint8_t b = tft_Read_8();

  #else

    // The 6 colour bits are in LS 6 bits of each byte but we do not include the extra clock pulse
    // so we use a trick and mask the middle 6 bits of the byte, then only shift 1 place left
    uint8_t r = (tft_Read_8()&0x7E)<<1;
    uint8_t g = (tft_Read_8()&0x7E)<<1;
    uint8_t b = (tft_Read_8()&0x7E)<<1;

  #endif

    #if defined (WROVER_KIT_LCD_DRIVER)
      // pushRect() is deprecated so don't care
      *data++ = color565(r, g, b);
    #else
      // Swapped colour byte order for compatibility with pushRect()
      *data++ = (r & 0xF8) | (g & 0xE0) >> 5 | (b & 0xF8) << 5 | (g & 0x1C) << 11;
    #endif
  }

  CS_H;

  #ifdef TFT_SDA_READ
    end_SDA_Read();
  #endif

  end_tft_read();

#endif
}


/***************************************************************************************
** Function name:           push rectangle (for SPI Interface II i.e. IM [3:0] = "1101")
** Description:             push 565 pixel colours into a defined area
***************************************************************************************/
void TFT_eSPI::pushRect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data)
{
  // Function deprecated, remains for backwards compatibility
  // pushImage() is better as it will crop partly off-screen image blocks
  pushImage(x, y, w, h, data);
}


/***************************************************************************************
** Function name:           pushImage
** Description:             plot 16 bit colour sprite or image onto TFT
***************************************************************************************/
void TFT_eSPI::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data)
{

  if ((x >= _width) || (y >= _height)) return;

  int32_t dx = 0;
  int32_t dy = 0;
  int32_t dw = w;
  int32_t dh = h;

  if (x < 0) { dw += x; dx = -x; x = 0; }
  if (y < 0) { dh += y; dy = -y; y = 0; }

  if ((x + dw) > _width ) dw = _width  - x;
  if ((y + dh) > _height) dh = _height - y;

  if (dw < 1 || dh < 1) return;

  begin_tft_write();
  inTransaction = true;

  setWindow(x, y, x + dw - 1, y + dh - 1);

  data += dx + dy * w;

  while (dh--)
  {
    pushColors(data, dw, _swapBytes);
    data += w;
  }

  inTransaction = false;
  end_tft_write();
}

/***************************************************************************************
** Function name:           pushImage
** Description:             plot 16 bit sprite or image with 1 colour being transparent
***************************************************************************************/
void TFT_eSPI::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data, uint16_t transp)
{

  if ((x >= _width) || (y >= _height)) return;

  int32_t dx = 0;
  int32_t dy = 0;
  int32_t dw = w;
  int32_t dh = h;

  if (x < 0) { dw += x; dx = -x; x = 0; }
  if (y < 0) { dh += y; dy = -y; y = 0; }

  if ((x + dw) > _width ) dw = _width  - x;
  if ((y + dh) > _height) dh = _height - y;

  if (dw < 1 || dh < 1) return;

  begin_tft_write();
  inTransaction = true;

  data += dx + dy * w;

  int32_t xe = x + dw - 1, ye = y + dh - 1;

  uint16_t  lineBuf[dw];

  if (!_swapBytes) transp = transp >> 8 | transp << 8;

  while (dh--)
  {
    int32_t len = dw;
    uint16_t* ptr = data;
    int32_t px = x;
    boolean move = true;
    uint16_t np = 0;

    while (len--)
    {
      if (transp != *ptr)
      {
        if (move) { move = false; setWindow(px, y, xe, ye); }
        lineBuf[np] = *ptr;
        np++;
      }
      else
      {
        move = true;
        if (np)
        {
           pushColors((uint16_t*)lineBuf, np, _swapBytes);
           np = 0;
        }
      }
      px++;
      ptr++;
    }
    if (np) pushColors((uint16_t*)lineBuf, np, _swapBytes);

    y++;
    data += w;
  }

  inTransaction = false;
  end_tft_write();
}


/***************************************************************************************
** Function name:           pushImage - for FLASH (PROGMEM) stored images
** Description:             plot 16 bit image
***************************************************************************************/
#define PI_BUF_SIZE 128
void TFT_eSPI::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data)
{
  // Requires 32 bit aligned access, so use PROGMEM 16 bit word functions
  if ((x >= _width) || (y >= _height)) return;

  int32_t dx = 0;
  int32_t dy = 0;
  int32_t dw = w;
  int32_t dh = h;

  if (x < 0) { dw += x; dx = -x; x = 0; }
  if (y < 0) { dh += y; dy = -y; y = 0; }

  if ((x + dw) > _width ) dw = _width  - x;
  if ((y + dh) > _height) dh = _height - y;

  if (dw < 1 || dh < 1) return;

  begin_tft_write();
  inTransaction = true;

  data += dx + dy * w;

  uint16_t  buffer[PI_BUF_SIZE];
  uint16_t* pix_buffer = buffer;

  setWindow(x, y, x + dw - 1, y + dh - 1);

  // Work out the number whole buffers to send
  uint16_t nb = (dw * dh) / PI_BUF_SIZE;

  // Fill and send "nb" buffers to TFT
  for (int32_t i = 0; i < nb; i++) {
    for (int32_t j = 0; j < PI_BUF_SIZE; j++) {
      pix_buffer[j] = pgm_read_word(&data[i * PI_BUF_SIZE + j]);
    }
    pushColors(pix_buffer, PI_BUF_SIZE, _swapBytes);
  }

  // Work out number of pixels not yet sent
  uint16_t np = (dw * dh) % PI_BUF_SIZE;

  // Send any partial buffer left over
  if (np) {
    for (int32_t i = 0; i < np; i++)
    {
      pix_buffer[i] = pgm_read_word(&data[nb * PI_BUF_SIZE + i]);
    }
    pushColors(pix_buffer, np, _swapBytes);
  }

  inTransaction = false;
  end_tft_write();
}


/***************************************************************************************
** Function name:           pushImage - for FLASH (PROGMEM) stored images
** Description:             plot 16 bit image with 1 colour being transparent
***************************************************************************************/
void TFT_eSPI::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data, uint16_t transp)
{
  // Requires 32 bit aligned access, so use PROGMEM 16 bit word functions
  if ((x >= _width) || (y >= (int32_t)_height)) return;

  int32_t dx = 0;
  int32_t dy = 0;
  int32_t dw = w;
  int32_t dh = h;

  if (x < 0) { dw += x; dx = -x; x = 0; }
  if (y < 0) { dh += y; dy = -y; y = 0; }

  if ((x + dw) > _width ) dw = _width  - x;
  if ((y + dh) > _height) dh = _height - y;

  if (dw < 1 || dh < 1) return;

  begin_tft_write();
  inTransaction = true;

  data += dx + dy * w;

  int32_t xe = x + dw - 1, ye = y + dh - 1;

  uint16_t  lineBuf[dw];

  if (!_swapBytes) transp = transp >> 8 | transp << 8;

  while (dh--) {
    int32_t len = dw;
    uint16_t* ptr = (uint16_t*)data;
    int32_t px = x;
    bool move = true;

    uint16_t np = 0;

    while (len--) {
      uint16_t color = pgm_read_word(ptr);
      if (transp != color) {
        if (move) { move = false; setWindow(px, y, xe, ye); }
        lineBuf[np] = color;
        np++;
      }
      else {
        move = true;
        if (np) {
           pushColors(lineBuf, np, _swapBytes);
           np = 0;
        }
      }
      px++;
      ptr++;
    }
    if (np) pushColors(lineBuf, np, _swapBytes);

    y++;
    data += w;
  }

  inTransaction = false;
  end_tft_write();
}


/***************************************************************************************
** Function name:           pushImage
** Description:             plot 8 bit or 4 bit or 1 bit image or sprite using a line buffer
***************************************************************************************/
void TFT_eSPI::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data, bool bpp8,  uint16_t *cmap)
{

  if ((x >= _width) || (y >= (int32_t)_height)) return;

  int32_t dx = 0;
  int32_t dy = 0;
  int32_t dw = w;
  int32_t dh = h;

  if (x < 0) { dw += x; dx = -x; x = 0; }
  if (y < 0) { dh += y; dy = -y; y = 0; }

  if ((x + dw) > _width ) dw = _width  - x;
  if ((y + dh) > _height) dh = _height - y;

  if (dw < 1 || dh < 1) return;

  begin_tft_write();
  inTransaction = true;

  setWindow(x, y, x + dw - 1, y + dh - 1); // Sets CS low and sent RAMWR

  // Line buffer makes plotting faster
  uint16_t  lineBuf[dw];

  if (bpp8)
  {
    uint8_t  blue[] = {0, 11, 21, 31}; // blue 2 to 5 bit colour lookup table

    _lastColor = -1; // Set to illegal value

    // Used to store last shifted colour
    uint8_t msbColor = 0;
    uint8_t lsbColor = 0;

    data += dx + dy * w;
    while (dh--) {
      uint32_t len = dw;
      uint8_t* ptr = data;
      uint8_t* linePtr = (uint8_t*)lineBuf;

      while(len--) {
        uint32_t color = *ptr++;

        // Shifts are slow so check if colour has changed first
        if (color != _lastColor) {
          //          =====Green=====     ===============Red==============
          msbColor = (color & 0x1C)>>2 | (color & 0xC0)>>3 | (color & 0xE0);
          //          =====Green=====    =======Blue======
          lsbColor = (color & 0x1C)<<3 | blue[color & 0x03];
          _lastColor = color;
        }

       *linePtr++ = msbColor;
       *linePtr++ = lsbColor;
      }

      pushPixels(lineBuf, dw);

      data += w;
    }
  }
  else if (cmap != nullptr)
  {
    bool swap = _swapBytes; _swapBytes = true;

    w = (w+1) & 0xFFFE;   // if this is a sprite, w will already be even; this does no harm.
    bool splitFirst = (dx & 0x01) != 0; // split first means we have to push a single px from the left of the sprite / image

    if (splitFirst) {
      data += ((dx - 1 + dy * w) >> 1);
    }
    else {
      data += ((dx + dy * w) >> 1);
    }

    while (dh--) {
      uint32_t len = dw;
      uint8_t * ptr = data;
      uint16_t *linePtr = lineBuf;
      uint8_t colors; // two colors in one byte
      uint16_t index;

      if (splitFirst) {
        colors = *ptr;
        index = (colors & 0x0F);
        *linePtr++ = cmap[index];
        len--;
        ptr++;
      }

      while (len--)
      {
        colors = *ptr;
        index = ((colors & 0xF0) >> 4) & 0x0F;
        *linePtr++ = cmap[index];

        if (len--)
        {
          index = colors & 0x0F;
          *linePtr++ = cmap[index];
        } else {
          break;  // nothing to do here
        }

        ptr++;
      }

      pushPixels(lineBuf, dw);
      data += (w >> 1);
    }
    _swapBytes = swap; // Restore old value
  }
  else
  {
    while (dh--) {
      w =  (w+7) & 0xFFF8;

      int32_t len = dw;
      uint8_t* ptr = data;
      uint8_t* linePtr = (uint8_t*)lineBuf;
      uint8_t  bits = 8;
      while(len>0) {
        if (len < 8) bits = len;
        uint32_t xp = dx;
        for (uint16_t i = 0; i < bits; i++) {
          uint8_t col = (ptr[(xp + dy * w)>>3] << (xp & 0x7)) & 0x80;
          if (col) {*linePtr++ = bitmap_fg>>8; *linePtr++ = (uint8_t) bitmap_fg;}
          else     {*linePtr++ = bitmap_bg>>8; *linePtr++ = (uint8_t) bitmap_bg;}
          //if (col) drawPixel((dw-len)+xp,h-dh,bitmap_fg);
          //else     drawPixel((dw-len)+xp,h-dh,bitmap_bg);
          xp++;
        }
        ptr++;
        len -= 8;
      }

      pushPixels(lineBuf, dw);

      dy++;
    }
  }

  inTransaction = false;
  end_tft_write();
}


/***************************************************************************************
** Function name:           pushImage
** Description:             plot 8 or 4 or 1 bit image or sprite with a transparent colour
***************************************************************************************/
void TFT_eSPI::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data, uint8_t transp, bool bpp8, uint16_t *cmap)
{
  if ((x >= _width) || (y >= _height)) return;

  int32_t dx = 0;
  int32_t dy = 0;
  int32_t dw = w;
  int32_t dh = h;

  if (x < 0) { dw += x; dx = -x; x = 0; }
  if (y < 0) { dh += y; dy = -y; y = 0; }

  if ((x + dw) > _width ) dw = _width  - x;
  if ((y + dh) > _height) dh = _height - y;

  if (dw < 1 || dh < 1) return;

  begin_tft_write();
  inTransaction = true;

  int32_t xe = x + dw - 1, ye = y + dh - 1;

  // Line buffer makes plotting faster
  uint16_t  lineBuf[dw];

  if (bpp8) { // 8 bits per pixel
    data += dx + dy * w;

    uint8_t  blue[] = {0, 11, 21, 31}; // blue 2 to 5 bit colour lookup table

    _lastColor = -1; // Set to illegal value

    // Used to store last shifted colour
    uint8_t msbColor = 0;
    uint8_t lsbColor = 0;

    //int32_t spx = x, spy = y;

    while (dh--) {
      int32_t len = dw;
      uint8_t* ptr = data;
      uint8_t* linePtr = (uint8_t*)lineBuf;

      int32_t px = x;
      bool move = true;
      uint16_t np = 0;

      while (len--) {
        if (transp != *ptr) {
          if (move) { move = false; setWindow(px, y, xe, ye);}
          uint8_t color = *ptr;

          // Shifts are slow so check if colour has changed first
          if (color != _lastColor) {
            //          =====Green=====     ===============Red==============
            msbColor = (color & 0x1C)>>2 | (color & 0xC0)>>3 | (color & 0xE0);
            //          =====Green=====    =======Blue======
            lsbColor = (color & 0x1C)<<3 | blue[color & 0x03];
            _lastColor = color;
          }
          *linePtr++ = msbColor;
          *linePtr++ = lsbColor;
          np++;
        }
        else {
          move = true;
          if (np) {
            pushPixels(lineBuf, np);
            linePtr = (uint8_t*)lineBuf;
            np = 0;
          }
        }
        px++;
        ptr++;
      }

      if (np) pushPixels(lineBuf, np);
      y++;
      data += w;
    }
  }
  else if (cmap != nullptr) // 4bpp with color map
  {
    bool swap = _swapBytes; _swapBytes = true;

    w = (w+1) & 0xFFFE; // here we try to recreate iwidth from dwidth.
    bool splitFirst = ((dx & 0x01) != 0);
    if (splitFirst) {
      data += ((dx - 1 + dy * w) >> 1);
    }
    else {
      data += ((dx + dy * w) >> 1);
    }

    while (dh--) {
      uint32_t len = dw;
      uint8_t * ptr = data;

      int32_t px = x;
      bool move = true;
      uint16_t np = 0;

      uint8_t index;  // index into cmap.

      if (splitFirst) {
        index = (*ptr & 0x0F);  // odd = bits 3 .. 0
        if (index != transp) {
          move = false; setWindow(px, y, xe, ye);
          lineBuf[np] = cmap[index];
          np++;
        }
        px++; ptr++;
        len--;
      }

      while (len--)
      {
        uint8_t color = *ptr;

        // find the actual color you care about.  There will be two pixels here!
        // but we may only want one at the end of the row
        uint16_t index = ((color & 0xF0) >> 4) & 0x0F;  // high bits are the even numbers
        if (index != transp) {
          if (move) {
            move = false; setWindow(px, y, xe, ye);
          }
          lineBuf[np] = cmap[index];
          np++; // added a pixel
        }
        else {
          move = true;
          if (np) {
            pushPixels(lineBuf, np);
            np = 0;
          }
        }
        px++;

        if (len--)
        {
          index = color & 0x0F; // the odd number is 3 .. 0
          if (index != transp) {
            if (move) {
              move = false; setWindow(px, y, xe, ye);
             }
            lineBuf[np] = cmap[index];
            np++;
          }
          else {
            move = true;
            if (np) {
              pushPixels(lineBuf, np);
              np = 0;
            }
          }
          px++;
        }
        else {
          break;  // we are done with this row.
        }
        ptr++;  // we only increment ptr once in the loop (deliberate)
      }

      if (np) {
        pushPixels(lineBuf, np);
        np = 0;
      }
      data += (w>>1);
      y++;
    }
    _swapBytes = swap; // Restore old value
  }
  else { // 1 bit per pixel
    w =  (w+7) & 0xFFF8;
    while (dh--) {
      int32_t px = x;
      bool move = true;
      uint16_t np = 0;
      int32_t len = dw;
      uint8_t* ptr = data;
      uint8_t  bits = 8;
      while(len>0) {
        if (len < 8) bits = len;
        uint32_t xp = dx;
        uint32_t yp = (dy * w)>>3;
        for (uint16_t i = 0; i < bits; i++) {
          //uint8_t col = (ptr[(xp + dy * w)>>3] << (xp & 0x7)) & 0x80;
          if ((ptr[(xp>>3) + yp] << (xp & 0x7)) & 0x80) {
            if (move) {
              move = false;
              setWindow(px, y, xe, ye);
            }
            np++;
          }
          else {
            if (np) {
              pushBlock(bitmap_fg, np);
              np = 0;
              move = true;
            }
          }
          px++;
          xp++;
        }
        ptr++;
        len -= 8;
      }

      if (np) pushBlock(bitmap_fg, np);
      y++;
      dy++;
    }
  }

  inTransaction = false;
  end_tft_write();
}




/***************************************************************************************
** Function name:           setSwapBytes
** Description:             Used by 16 bit pushImage() to swap byte order in colours
***************************************************************************************/
void TFT_eSPI::setSwapBytes(bool swap)
{
  _swapBytes = swap;
}


/***************************************************************************************
** Function name:           getSwapBytes
** Description:             Return the swap byte order for colours
***************************************************************************************/
bool TFT_eSPI::getSwapBytes(void)
{
  return _swapBytes;
}

/***************************************************************************************
** Function name:           read rectangle (for SPI Interface II i.e. IM [3:0] = "1101")
** Description:             Read RGB pixel colours from a defined area
***************************************************************************************/
// If w and h are 1, then 1 pixel is read, *data array size must be 3 bytes per pixel
void  TFT_eSPI::readRectRGB(int32_t x0, int32_t y0, int32_t w, int32_t h, uint8_t *data)
{
#if defined(ESP32_PARALLEL)

  uint32_t len = w * h;
  uint8_t* buf565 = data + len;

  readRect(x0, y0, w, h, (uint16_t*)buf565);

  while (len--) {
    uint16_t pixel565 = (*buf565++)<<8 | (*buf565++);
    uint8_t red   = (pixel565 & 0xF800) >> 8; red   |= red   >> 5;
    uint8_t green = (pixel565 & 0x07E0) >> 3; green |= green >> 6;
    uint8_t blue  = (pixel565 & 0x001F) << 3; blue  |= blue  >> 5;
    *data++ = red;
    *data++ = green;
    *data++ = blue;
  }

#else  // Not ESP32_PARALLEL

  begin_tft_read();

  readAddrWindow(x0, y0, w, h); // Sets CS low

  #ifdef TFT_SDA_READ
    begin_SDA_Read();
  #endif

  // Dummy read to throw away don't care value
  tft_Read_8();
  #if defined (WROVER_KIT_LCD_DRIVER)
    tft_Read_8(); // Extra dummy read for WROVER_KIT_LCD_DRIVER (and possibly other ST7789)
  #endif
  // Read window pixel 24 bit RGB values, buffer must be set in sketch to 3 * w * h
  uint32_t len = w * h;
  while (len-->0) {

  #if !defined (ILI9488_DRIVER)

    // Read the 3 RGB bytes, colour is actually only in the top 6 bits of each byte
    // as the TFT stores colours as 18 bits
    *data++ = tft_Read_8();
    *data++ = tft_Read_8();
    *data++ = tft_Read_8();

  #else

    // The 6 colour bits are in MS 6 bits of each byte, but the ILI9488 needs an extra clock pulse
    // so bits appear shifted right 1 bit, so mask the middle 6 bits then shift 1 place left
    *data++ = (tft_Read_8()&0x7E)<<1;
    *data++ = (tft_Read_8()&0x7E)<<1;
    *data++ = (tft_Read_8()&0x7E)<<1;

  #endif

  }

  CS_H;

  #ifdef TFT_SDA_READ
    end_SDA_Read();
  #endif

  end_tft_read();

#endif
}


/***************************************************************************************
** Function name:           drawCircle
** Description:             Draw a circle outline
***************************************************************************************/
// Optimised midpoint circle algorithm
void TFT_eSPI::drawCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color)
{
  int32_t  x  = 0;
  int32_t  dx = 1;
  int32_t  dy = r+r;
  int32_t  p  = -(r>>1);

  //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
  inTransaction = true;

  // These are ordered to minimise coordinate changes in x or y
  // drawPixel can then send fewer bounding box commands
  drawPixel(x0 + r, y0, color);
  drawPixel(x0 - r, y0, color);
  drawPixel(x0, y0 - r, color);
  drawPixel(x0, y0 + r, color);

  while(x<r){

    if(p>=0) {
      dy-=2;
      p-=dy;
      r--;
    }

    dx+=2;
    p+=dx;

    x++;

    // These are ordered to minimise coordinate changes in x or y
    // drawPixel can then send fewer bounding box commands
    drawPixel(x0 + x, y0 + r, color);
    drawPixel(x0 - x, y0 + r, color);
    drawPixel(x0 - x, y0 - r, color);
    drawPixel(x0 + x, y0 - r, color);

    drawPixel(x0 + r, y0 + x, color);
    drawPixel(x0 - r, y0 + x, color);
    drawPixel(x0 - r, y0 - x, color);
    drawPixel(x0 + r, y0 - x, color);
  }

  inTransaction = false;
  end_tft_write();              // Does nothing if Sprite class uses this function
}


/***************************************************************************************
** Function name:           drawCircleHelper
** Description:             Support function for circle drawing
***************************************************************************************/
void TFT_eSPI::drawCircleHelper( int32_t x0, int32_t y0, int32_t r, uint8_t cornername, uint32_t color)
{
  int32_t f     = 1 - r;
  int32_t ddF_x = 1;
  int32_t ddF_y = -2 * r;
  int32_t x     = 0;

  while (x < r) {
    if (f >= 0) {
      r--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;
    if (cornername & 0x4) {
      drawPixel(x0 + x, y0 + r, color);
      drawPixel(x0 + r, y0 + x, color);
    }
    if (cornername & 0x2) {
      drawPixel(x0 + x, y0 - r, color);
      drawPixel(x0 + r, y0 - x, color);
    }
    if (cornername & 0x8) {
      drawPixel(x0 - r, y0 + x, color);
      drawPixel(x0 - x, y0 + r, color);
    }
    if (cornername & 0x1) {
      drawPixel(x0 - r, y0 - x, color);
      drawPixel(x0 - x, y0 - r, color);
    }
  }
}


/***************************************************************************************
** Function name:           fillCircle
** Description:             draw a filled circle
***************************************************************************************/
// Optimised midpoint circle algorithm, changed to horizontal lines (faster in sprites)
void TFT_eSPI::fillCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color)
{
  int32_t  x  = 0;
  int32_t  dx = 1;
  int32_t  dy = r+r;
  int32_t  p  = -(r>>1);

  //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
  inTransaction = true;

  drawFastHLine(x0 - r, y0, dy+1, color);

  while(x<r){

    if(p>=0) {
      dy-=2;
      p-=dy;
      r--;
    }

    dx+=2;
    p+=dx;

    x++;

    drawFastHLine(x0 - r, y0 + x, 2 * r+1, color);
    drawFastHLine(x0 - r, y0 - x, 2 * r+1, color);
    drawFastHLine(x0 - x, y0 + r, 2 * x+1, color);
    drawFastHLine(x0 - x, y0 - r, 2 * x+1, color);

  }

  inTransaction = false;
  end_tft_write();              // Does nothing if Sprite class uses this function
}


/***************************************************************************************
** Function name:           fillCircleHelper
** Description:             Support function for filled circle drawing
***************************************************************************************/
// Used to support drawing roundrects, changed to horizontal lines (faster in sprites)
void TFT_eSPI::fillCircleHelper(int32_t x0, int32_t y0, int32_t r, uint8_t cornername, int32_t delta, uint32_t color)
{
  int32_t f     = 1 - r;
  int32_t ddF_x = 1;
  int32_t ddF_y = -r - r;
  int32_t y     = 0;

  delta++;
  while (y < r) {
    if (f >= 0) {
      r--;
      ddF_y += 2;
      f     += ddF_y;
    }
    y++;
    //x++;
    ddF_x += 2;
    f     += ddF_x;

    if (cornername & 0x1)
    {
      drawFastHLine(x0 - r, y0 + y, r + r + delta, color);
      drawFastHLine(x0 - y, y0 + r, y + y + delta, color);
    }
    if (cornername & 0x2) {
      drawFastHLine(x0 - r, y0 - y, r + r + delta, color); // 11995, 1090
      drawFastHLine(x0 - y, y0 - r, y + y + delta, color);
    }
  }
}


/***************************************************************************************
** Function name:           drawEllipse
** Description:             Draw a ellipse outline
***************************************************************************************/
void TFT_eSPI::drawEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color)
{
  if (rx<2) return;
  if (ry<2) return;
  int32_t x, y;
  int32_t rx2 = rx * rx;
  int32_t ry2 = ry * ry;
  int32_t fx2 = 4 * rx2;
  int32_t fy2 = 4 * ry2;
  int32_t s;

  //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
  inTransaction = true;

  for (x = 0, y = ry, s = 2*ry2+rx2*(1-2*ry); ry2*x <= rx2*y; x++)
  {
    // These are ordered to minimise coordinate changes in x or y
    // drawPixel can then send fewer bounding box commands
    drawPixel(x0 + x, y0 + y, color);
    drawPixel(x0 - x, y0 + y, color);
    drawPixel(x0 - x, y0 - y, color);
    drawPixel(x0 + x, y0 - y, color);
    if (s >= 0)
    {
      s += fx2 * (1 - y);
      y--;
    }
    s += ry2 * ((4 * x) + 6);
  }

  for (x = rx, y = 0, s = 2*rx2+ry2*(1-2*rx); rx2*y <= ry2*x; y++)
  {
    // These are ordered to minimise coordinate changes in x or y
    // drawPixel can then send fewer bounding box commands
    drawPixel(x0 + x, y0 + y, color);
    drawPixel(x0 - x, y0 + y, color);
    drawPixel(x0 - x, y0 - y, color);
    drawPixel(x0 + x, y0 - y, color);
    if (s >= 0)
    {
      s += fy2 * (1 - x);
      x--;
    }
    s += rx2 * ((4 * y) + 6);
  }

  inTransaction = false;
  end_tft_write();              // Does nothing if Sprite class uses this function
}


/***************************************************************************************
** Function name:           fillEllipse
** Description:             draw a filled ellipse
***************************************************************************************/
void TFT_eSPI::fillEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color)
{
  if (rx<2) return;
  if (ry<2) return;
  int32_t x, y;
  int32_t rx2 = rx * rx;
  int32_t ry2 = ry * ry;
  int32_t fx2 = 4 * rx2;
  int32_t fy2 = 4 * ry2;
  int32_t s;

  //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
  inTransaction = true;

  for (x = 0, y = ry, s = 2*ry2+rx2*(1-2*ry); ry2*x <= rx2*y; x++)
  {
    drawFastHLine(x0 - x, y0 - y, x + x + 1, color);
    drawFastHLine(x0 - x, y0 + y, x + x + 1, color);

    if (s >= 0)
    {
      s += fx2 * (1 - y);
      y--;
    }
    s += ry2 * ((4 * x) + 6);
  }

  for (x = rx, y = 0, s = 2*rx2+ry2*(1-2*rx); rx2*y <= ry2*x; y++)
  {
    drawFastHLine(x0 - x, y0 - y, x + x + 1, color);
    drawFastHLine(x0 - x, y0 + y, x + x + 1, color);

    if (s >= 0)
    {
      s += fy2 * (1 - x);
      x--;
    }
    s += rx2 * ((4 * y) + 6);
  }

  inTransaction = false;
  end_tft_write();              // Does nothing if Sprite class uses this function
}


/***************************************************************************************
** Function name:           fillScreen
** Description:             Clear the screen to defined colour
***************************************************************************************/
void TFT_eSPI::fillScreen(uint32_t color)
{
  fillRect(0, 0, _width, _height, color);
}


/***************************************************************************************
** Function name:           drawRect
** Description:             Draw a rectangle outline
***************************************************************************************/
// Draw a rectangle
void TFT_eSPI::drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)
{
  //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
  inTransaction = true;

  drawFastHLine(x, y, w, color);
  drawFastHLine(x, y + h - 1, w, color);
  // Avoid drawing corner pixels twice
  drawFastVLine(x, y+1, h-2, color);
  drawFastVLine(x + w - 1, y+1, h-2, color);

  inTransaction = false;
  end_tft_write();              // Does nothing if Sprite class uses this function
}


/***************************************************************************************
** Function name:           drawRoundRect
** Description:             Draw a rounded corner rectangle outline
***************************************************************************************/
// Draw a rounded rectangle
void TFT_eSPI::drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color)
{
  //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
  inTransaction = true;

  // smarter version
  drawFastHLine(x + r  , y    , w - r - r, color); // Top
  drawFastHLine(x + r  , y + h - 1, w - r - r, color); // Bottom
  drawFastVLine(x    , y + r  , h - r - r, color); // Left
  drawFastVLine(x + w - 1, y + r  , h - r - r, color); // Right
  // draw four corners
  drawCircleHelper(x + r    , y + r    , r, 1, color);
  drawCircleHelper(x + w - r - 1, y + r    , r, 2, color);
  drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
  drawCircleHelper(x + r    , y + h - r - 1, r, 8, color);

  inTransaction = false;
  end_tft_write();              // Does nothing if Sprite class uses this function
}


/***************************************************************************************
** Function name:           fillRoundRect
** Description:             Draw a rounded corner filled rectangle
***************************************************************************************/
// Fill a rounded rectangle, changed to horizontal lines (faster in sprites)
void TFT_eSPI::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color)
{
  //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
  inTransaction = true;

  // smarter version
  fillRect(x, y + r, w, h - r - r, color);

  // draw four corners
  fillCircleHelper(x + r, y + h - r - 1, r, 1, w - r - r - 1, color);
  fillCircleHelper(x + r    , y + r, r, 2, w - r - r - 1, color);

  inTransaction = false;
  end_tft_write();              // Does nothing if Sprite class uses this function
}


/***************************************************************************************
** Function name:           drawTriangle
** Description:             Draw a triangle outline using 3 arbitrary points
***************************************************************************************/
// Draw a triangle
void TFT_eSPI::drawTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color)
{
  //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
  inTransaction = true;

  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color);

  inTransaction = false;
  end_tft_write();              // Does nothing if Sprite class uses this function
}


/***************************************************************************************
** Function name:           fillTriangle
** Description:             Draw a filled triangle using 3 arbitrary points
***************************************************************************************/
// Fill a triangle - original Adafruit function works well and code footprint is small
void TFT_eSPI::fillTriangle ( int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color)
{
  int32_t a, b, y, last;

  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1) {
    swap_coord(y0, y1); swap_coord(x0, x1);
  }
  if (y1 > y2) {
    swap_coord(y2, y1); swap_coord(x2, x1);
  }
  if (y0 > y1) {
    swap_coord(y0, y1); swap_coord(x0, x1);
  }

  if (y0 == y2) { // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if (x1 < a)      a = x1;
    else if (x1 > b) b = x1;
    if (x2 < a)      a = x2;
    else if (x2 > b) b = x2;
    drawFastHLine(a, y0, b - a + 1, color);
    return;
  }

  //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
  inTransaction = true;

  int32_t
  dx01 = x1 - x0,
  dy01 = y1 - y0,
  dx02 = x2 - x0,
  dy02 = y2 - y0,
  dx12 = x2 - x1,
  dy12 = y2 - y1,
  sa   = 0,
  sb   = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if (y1 == y2) last = y1;  // Include y1 scanline
  else         last = y1 - 1; // Skip it

  for (y = y0; y <= last; y++) {
    a   = x0 + sa / dy01;
    b   = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;

    if (a > b) swap_coord(a, b);
    drawFastHLine(a, y, b - a + 1, color);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for (; y <= y2; y++) {
    a   = x1 + sa / dy12;
    b   = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;

    if (a > b) swap_coord(a, b);
    drawFastHLine(a, y, b - a + 1, color);
  }

  inTransaction = false;
  end_tft_write();              // Does nothing if Sprite class uses this function
}


/***************************************************************************************
** Function name:           drawBitmap
** Description:             Draw an image stored in an array on the TFT
***************************************************************************************/
void TFT_eSPI::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color)
{
  //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
  inTransaction = true;

  int32_t i, j, byteWidth = (w + 7) / 8;

  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++ ) {
      if (pgm_read_byte(bitmap + j * byteWidth + i / 8) & (128 >> (i & 7))) {
        drawPixel(x + i, y + j, color);
      }
    }
  }

  inTransaction = false;
  end_tft_write();              // Does nothing if Sprite class uses this function
}


/***************************************************************************************
** Function name:           drawXBitmap
** Description:             Draw an image stored in an XBM array onto the TFT
***************************************************************************************/
void TFT_eSPI::drawXBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color)
{
  //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
  inTransaction = true;

  int32_t i, j, byteWidth = (w + 7) / 8;

  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++ ) {
      if (pgm_read_byte(bitmap + j * byteWidth + i / 8) & (1 << (i & 7))) {
        drawPixel(x + i, y + j, color);
      }
    }
  }

  inTransaction = false;
  end_tft_write();              // Does nothing if Sprite class uses this function
}


/***************************************************************************************
** Function name:           drawXBitmap
** Description:             Draw an XBM image with foreground and background colors
***************************************************************************************/
void TFT_eSPI::drawXBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bgcolor)
{
  //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
  inTransaction = true;

  int32_t i, j, byteWidth = (w + 7) / 8;

  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++ ) {
      if (pgm_read_byte(bitmap + j * byteWidth + i / 8) & (1 << (i & 7)))
           drawPixel(x + i, y + j,   color);
      else drawPixel(x + i, y + j, bgcolor);
    }
  }

  inTransaction = false;
  end_tft_write();              // Does nothing if Sprite class uses this function
}


/***************************************************************************************
** Function name:           setCursor
** Description:             Set the text cursor x,y position
***************************************************************************************/
void TFT_eSPI::setCursor(int16_t x, int16_t y)
{
  cursor_x = x;
  cursor_y = y;
}


/***************************************************************************************
** Function name:           setCursor
** Description:             Set the text cursor x,y position and font
***************************************************************************************/
void TFT_eSPI::setCursor(int16_t x, int16_t y, uint8_t font)
{
  textfont = font;
  cursor_x = x;
  cursor_y = y;
}


/***************************************************************************************
** Function name:           getCursorX
** Description:             Get the text cursor x position
***************************************************************************************/
int16_t TFT_eSPI::getCursorX(void)
{
  return cursor_x;
}

/***************************************************************************************
** Function name:           getCursorY
** Description:             Get the text cursor y position
***************************************************************************************/
int16_t TFT_eSPI::getCursorY(void)
{
  return cursor_y;
}


/***************************************************************************************
** Function name:           setTextSize
** Description:             Set the text size multiplier
***************************************************************************************/
void TFT_eSPI::setTextSize(uint8_t s)
{
  if (s>7) s = 7; // Limit the maximum size multiplier so byte variables can be used for rendering
  textsize = (s > 0) ? s : 1; // Don't allow font size 0
}


/***************************************************************************************
** Function name:           setTextColor
** Description:             Set the font foreground colour (background is transparent)
***************************************************************************************/
void TFT_eSPI::setTextColor(uint16_t c)
{
  // For 'transparent' background, we'll set the bg
  // to the same as fg instead of using a flag
  textcolor = textbgcolor = c;
}


/***************************************************************************************
** Function name:           setTextColor
** Description:             Set the font foreground and background colour
***************************************************************************************/
void TFT_eSPI::setTextColor(uint16_t c, uint16_t b)
{
  textcolor   = c;
  textbgcolor = b;
}


/***************************************************************************************
** Function name:           setPivot
** Description:             Set the pivot point on the TFT
*************************************************************************************x*/
void TFT_eSPI::setPivot(int16_t x, int16_t y)
{
  _xpivot = x;
  _ypivot = y;
}


/***************************************************************************************
** Function name:           getPivotX
** Description:             Get the x pivot position
***************************************************************************************/
int16_t TFT_eSPI::getPivotX(void)
{
  return _xpivot;
}


/***************************************************************************************
** Function name:           getPivotY
** Description:             Get the y pivot position
***************************************************************************************/
int16_t TFT_eSPI::getPivotY(void)
{
  return _ypivot;
}


/***************************************************************************************
** Function name:           setBitmapColor
** Description:             Set the foreground foreground and background colour
***************************************************************************************/
void TFT_eSPI::setBitmapColor(uint16_t c, uint16_t b)
{
  if (c == b) b = ~c;
  bitmap_fg = c;
  bitmap_bg = b;
}


/***************************************************************************************
** Function name:           setTextWrap
** Description:             Define if text should wrap at end of line
***************************************************************************************/
void TFT_eSPI::setTextWrap(boolean wrapX, boolean wrapY)
{
  textwrapX = wrapX;
  textwrapY = wrapY;
}


/***************************************************************************************
** Function name:           setTextDatum
** Description:             Set the text position reference datum
***************************************************************************************/
void TFT_eSPI::setTextDatum(uint8_t d)
{
  textdatum = d;
}


/***************************************************************************************
** Function name:           setTextPadding
** Description:             Define padding width (aids erasing old text and numbers)
***************************************************************************************/
void TFT_eSPI::setTextPadding(uint16_t x_width)
{
  padX = x_width;
}


/***************************************************************************************
** Function name:           getRotation
** Description:             Return the rotation value (as used by setRotation())
***************************************************************************************/
uint8_t TFT_eSPI::getRotation(void)
{
  return rotation;
}

/***************************************************************************************
** Function name:           getTextDatum
** Description:             Return the text datum value (as used by setTextDatum())
***************************************************************************************/
uint8_t TFT_eSPI::getTextDatum(void)
{
  return textdatum;
}


/***************************************************************************************
** Function name:           width
** Description:             Return the pixel width of display (per current rotation)
***************************************************************************************/
// Return the size of the display (per current rotation)
int16_t TFT_eSPI::width(void)
{
  return _width;
}


/***************************************************************************************
** Function name:           height
** Description:             Return the pixel height of display (per current rotation)
***************************************************************************************/
int16_t TFT_eSPI::height(void)
{
  return _height;
}


/***************************************************************************************
** Function name:           textWidth
** Description:             Return the width in pixels of a string in a given font
***************************************************************************************/
int16_t TFT_eSPI::textWidth(const String& string)
{
  int16_t len = string.length() + 2;
  char buffer[len];
  string.toCharArray(buffer, len);
  return textWidth(buffer, textfont);
}

int16_t TFT_eSPI::textWidth(const String& string, uint8_t font)
{
  int16_t len = string.length() + 2;
  char buffer[len];
  string.toCharArray(buffer, len);
  return textWidth(buffer, font);
}

int16_t TFT_eSPI::textWidth(const char *string)
{
  return textWidth(string, textfont);
}

int16_t TFT_eSPI::textWidth(const char *string, uint8_t font)
{
  int32_t str_width = 0;
  uint16_t uniCode  = 0;

#ifdef SMOOTH_FONT
  if(fontLoaded)
  {
    while (*string)
    {
      uniCode = decodeUTF8(*string++);
      if (uniCode)
      {
        if (uniCode == 0x20) str_width += gFont.spaceWidth;
        else
        {
          uint16_t gNum = 0;
          bool found = getUnicodeIndex(uniCode, &gNum);
          if (found)
          {
            if(str_width == 0 && gdX[gNum] < 0) str_width -= gdX[gNum];
            if (*string || isDigits) str_width += gxAdvance[gNum];
            else str_width += (gdX[gNum] + gWidth[gNum]);
          }
          else str_width += gFont.spaceWidth + 1;
        }
      }
    }
    isDigits = false;
    return str_width;
  }
#endif

  if (font>1 && font<9)
  {
    char *widthtable = (char *)pgm_read_dword( &(fontdata[font].widthtbl ) ) - 32; //subtract the 32 outside the loop

    while (*string)
    {
      uniCode = *(string++);
      if (uniCode > 31 && uniCode < 128)
      str_width += pgm_read_byte( widthtable + uniCode); // Normally we need to subtract 32 from uniCode
      else str_width += pgm_read_byte( widthtable + 32); // Set illegal character = space width
    }
  }
  else
  {

#ifdef LOAD_GFXFF
    if(gfxFont) // New font
    {
      while (*string)
      {
        uniCode = decodeUTF8(*string++);
        if ((uniCode >= pgm_read_word(&gfxFont->first)) && (uniCode <= pgm_read_word(&gfxFont->last )))
        {
          uniCode -= pgm_read_word(&gfxFont->first);
          GFXglyph *glyph  = &(((GFXglyph *)pgm_read_dword(&gfxFont->glyph))[uniCode]);
          // If this is not the  last character or is a digit then use xAdvance
          if (*string  || isDigits) str_width += pgm_read_byte(&glyph->xAdvance);
          // Else use the offset plus width since this can be bigger than xAdvance
          else str_width += ((int8_t)pgm_read_byte(&glyph->xOffset) + pgm_read_byte(&glyph->width));
        }
      }
    }
    else
#endif
    {
#ifdef LOAD_GLCD
      while (*string++) str_width += 6;
#endif
    }
  }
  isDigits = false;
  return str_width * textsize;
}


/***************************************************************************************
** Function name:           fontsLoaded
** Description:             return an encoded 16 bit value showing the fonts loaded
***************************************************************************************/
// Returns a value showing which fonts are loaded (bit N set =  Font N loaded)

uint16_t TFT_eSPI::fontsLoaded(void)
{
  return fontsloaded;
}


/***************************************************************************************
** Function name:           fontHeight
** Description:             return the height of a font (yAdvance for free fonts)
***************************************************************************************/
int16_t TFT_eSPI::fontHeight(int16_t font)
{
#ifdef SMOOTH_FONT
  if(fontLoaded) return gFont.yAdvance;
#endif

#ifdef LOAD_GFXFF
  if (font==1)
  {
    if(gfxFont) // New font
    {
      return pgm_read_byte(&gfxFont->yAdvance) * textsize;
    }
  }
#endif
  return pgm_read_byte( &fontdata[font].height ) * textsize;
}

int16_t TFT_eSPI::fontHeight(void)
{
  return fontHeight(textfont);
}

/***************************************************************************************
** Function name:           drawChar
** Description:             draw a single character in the Adafruit GLCD font
***************************************************************************************/
void TFT_eSPI::drawChar(int32_t x, int32_t y, uint16_t c, uint32_t color, uint32_t bg, uint8_t size)
{
  if ((x >= _width)            || // Clip right
      (y >= _height)           || // Clip bottom
      ((x + 6 * size - 1) < 0) || // Clip left
      ((y + 8 * size - 1) < 0))   // Clip top
    return;

  if (c < 32) return;
#ifdef LOAD_GLCD
//>>>>>>>>>>>>>>>>>>
#ifdef LOAD_GFXFF
  if(!gfxFont) { // 'Classic' built-in font
#endif
//>>>>>>>>>>>>>>>>>>

  boolean fillbg = (bg != color);

  if ((size==1) && fillbg)
  {
    uint8_t column[6];
    uint8_t mask = 0x1;
    begin_tft_write();

    setWindow(x, y, x+5, y+8);

    for (int8_t i = 0; i < 5; i++ ) column[i] = pgm_read_byte(font + (c * 5) + i);
    column[5] = 0;

    for (int8_t j = 0; j < 8; j++) {
      for (int8_t k = 0; k < 5; k++ ) {
        if (column[k] & mask) {tft_Write_16(color);}
        else {tft_Write_16(bg);}
      }
      mask <<= 1;
      tft_Write_16(bg);
    }

    end_tft_write();
  }
  else
  {
    //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
    inTransaction = true;
    for (int8_t i = 0; i < 6; i++ ) {
      uint8_t line;
      if (i == 5)
        line = 0x0;
      else
        line = pgm_read_byte(font + (c * 5) + i);

      if (size == 1) // default size
      {
        for (int8_t j = 0; j < 8; j++) {
          if (line & 0x1) drawPixel(x + i, y + j, color);
          line >>= 1;
        }
      }
      else {  // big size
        for (int8_t j = 0; j < 8; j++) {
          if (line & 0x1) fillRect(x + (i * size), y + (j * size), size, size, color);
          else if (fillbg) fillRect(x + i * size, y + j * size, size, size, bg);
          line >>= 1;
        }
      }
    }
    inTransaction = false;
    end_tft_write();              // Does nothing if Sprite class uses this function
  }

//>>>>>>>>>>>>>>>>>>>>>>>>>>>
#ifdef LOAD_GFXFF
  } else { // Custom font
#endif
//>>>>>>>>>>>>>>>>>>>>>>>>>>>
#endif // LOAD_GLCD

#ifdef LOAD_GFXFF
    // Filter out bad characters not present in font
    if ((c >= pgm_read_word(&gfxFont->first)) && (c <= pgm_read_word(&gfxFont->last )))
    {
      //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
      inTransaction = true;
//>>>>>>>>>>>>>>>>>>>>>>>>>>>

      c -= pgm_read_word(&gfxFont->first);
      GFXglyph *glyph  = &(((GFXglyph *)pgm_read_dword(&gfxFont->glyph))[c]);
      uint8_t  *bitmap = (uint8_t *)pgm_read_dword(&gfxFont->bitmap);

      uint32_t bo = pgm_read_word(&glyph->bitmapOffset);
      uint8_t  w  = pgm_read_byte(&glyph->width),
               h  = pgm_read_byte(&glyph->height);
               //xa = pgm_read_byte(&glyph->xAdvance);
      int8_t   xo = pgm_read_byte(&glyph->xOffset),
               yo = pgm_read_byte(&glyph->yOffset);
      uint8_t  xx, yy, bits=0, bit=0;
      int16_t  xo16 = 0, yo16 = 0;

      if(size > 1) {
        xo16 = xo;
        yo16 = yo;
      }

// Here we have 3 versions of the same function just for evaluation purposes
// Comment out the next two #defines to revert to the slower Adafruit implementation

// If FAST_LINE is defined then the free fonts are rendered using horizontal lines
// this makes rendering fonts 2-5 times faster. Particularly good for large fonts.
// This is an elegant solution since it still uses generic functions present in the
// stock library.

// If FAST_SHIFT is defined then a slightly faster (at least for AVR processors)
// shifting bit mask is used

// Free fonts don't look good when the size multiplier is >1 so we could remove
// code if this is not wanted and speed things up

#define FAST_HLINE
#define FAST_SHIFT
//FIXED_SIZE is an option in User_Setup.h that only works with FAST_LINE enabled

#ifdef FIXED_SIZE
      x+=xo; // Save 88 bytes of FLASH
      y+=yo;
#endif

#ifdef FAST_HLINE

  #ifdef FAST_SHIFT
      uint16_t hpc = 0; // Horizontal foreground pixel count
      for(yy=0; yy<h; yy++) {
        for(xx=0; xx<w; xx++) {
          if(bit == 0) {
            bits = pgm_read_byte(&bitmap[bo++]);
            bit  = 0x80;
          }
          if(bits & bit) hpc++;
          else {
           if (hpc) {
#ifndef FIXED_SIZE
              if(size == 1) drawFastHLine(x+xo+xx-hpc, y+yo+yy, hpc, color);
              else fillRect(x+(xo16+xx-hpc)*size, y+(yo16+yy)*size, size*hpc, size, color);
#else
              drawFastHLine(x+xx-hpc, y+yy, hpc, color);
#endif
              hpc=0;
            }
          }
          bit >>= 1;
        }
      // Draw pixels for this line as we are about to increment yy
        if (hpc) {
#ifndef FIXED_SIZE
          if(size == 1) drawFastHLine(x+xo+xx-hpc, y+yo+yy, hpc, color);
          else fillRect(x+(xo16+xx-hpc)*size, y+(yo16+yy)*size, size*hpc, size, color);
#else
          drawFastHLine(x+xx-hpc, y+yy, hpc, color);
#endif
          hpc=0;
        }
      }
  #else
      uint16_t hpc = 0; // Horizontal foreground pixel count
      for(yy=0; yy<h; yy++) {
        for(xx=0; xx<w; xx++) {
          if(!(bit++ & 7)) {
            bits = pgm_read_byte(&bitmap[bo++]);
          }
          if(bits & 0x80) hpc++;
          else {
            if (hpc) {
              if(size == 1) drawFastHLine(x+xo+xx-hpc, y+yo+yy, hpc, color);
              else fillRect(x+(xo16+xx-hpc)*size, y+(yo16+yy)*size, size*hpc, size, color);
              hpc=0;
            }
          }
          bits <<= 1;
        }
        // Draw pixels for this line as we are about to increment yy
        if (hpc) {
          if(size == 1) drawFastHLine(x+xo+xx-hpc, y+yo+yy, hpc, color);
          else fillRect(x+(xo16+xx-hpc)*size, y+(yo16+yy)*size, size*hpc, size, color);
          hpc=0;
        }
      }
  #endif

#else
      for(yy=0; yy<h; yy++) {
        for(xx=0; xx<w; xx++) {
          if(!(bit++ & 7)) {
            bits = pgm_read_byte(&bitmap[bo++]);
          }
          if(bits & 0x80) {
            if(size == 1) {
              drawPixel(x+xo+xx, y+yo+yy, color);
            } else {
              fillRect(x+(xo16+xx)*size, y+(yo16+yy)*size, size, size, color);
            }
          }
          bits <<= 1;
        }
      }
#endif
      inTransaction = false;
      end_tft_write();              // Does nothing if Sprite class uses this function
    }
#endif


#ifdef LOAD_GLCD
  #ifdef LOAD_GFXFF
  } // End classic vs custom font
  #endif
#endif

}


/***************************************************************************************
** Function name:           setAddrWindow
** Description:             define an area to receive a stream of pixels
***************************************************************************************/
// Chip select is high at the end of this function
void TFT_eSPI::setAddrWindow(int32_t x0, int32_t y0, int32_t w, int32_t h)
{
  begin_tft_write();

  setWindow(x0, y0, x0 + w - 1, y0 + h - 1);

  end_tft_write();
}


/***************************************************************************************
** Function name:           setWindow
** Description:             define an area to receive a stream of pixels
***************************************************************************************/
// Chip select stays low, call spi_begin first. Use setAddrWindow() from sketches
void TFT_eSPI::setWindow(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
  //begin_tft_write(); // Must be called before setWimdow

  addr_col = 0xFFFF;
  addr_row = 0xFFFF;

#ifdef CGRAM_OFFSET
  x0+=colstart;
  x1+=colstart;
  y0+=rowstart;
  y1+=rowstart;
#endif

  DC_C; tft_Write_8(TFT_CASET);
  DC_D; tft_Write_32C(x0, x1);

  // Row addr set
  DC_C; tft_Write_8(TFT_PASET);
  DC_D; tft_Write_32C(y0, y1);

  // write to RAM
  DC_C; tft_Write_8(TFT_RAMWR);

  DC_D;

  //end_tft_write();
}



/***************************************************************************************
** Function name:           readAddrWindow
** Description:             define an area to read a stream of pixels
***************************************************************************************/
// Chip select stays low
void TFT_eSPI::readAddrWindow(int32_t xs, int32_t ys, int32_t w, int32_t h)
{

  int32_t xe = xs + w - 1;
  int32_t ye = ys + h - 1;

  addr_col = 0xFFFF;
  addr_row = 0xFFFF;

  #ifdef CGRAM_OFFSET
    xs += colstart;
    xe += colstart;
    ys += rowstart;
    ye += rowstart;
  #endif

  // Column addr set
  DC_C; tft_Write_8(TFT_CASET);
  DC_D; tft_Write_32C(xs, xe);

  // Row addr set
  DC_C; tft_Write_8(TFT_PASET);
  DC_D; tft_Write_32C(ys, ye);

  // Read CGRAM command
  DC_C; tft_Write_8(TFT_RAMRD);

  DC_D;

  //end_tft_write(); // Must be called after readAddrWindow or CS set high
}



/***************************************************************************************
** Function name:           drawPixel
** Description:             push a single pixel at an arbitrary position
***************************************************************************************/
void TFT_eSPI::drawPixel(int32_t x, int32_t y, uint32_t color)
{
  // Range checking
  if ((x < 0) || (y < 0) ||(x >= _width) || (y >= _height)) return;

  begin_tft_write();

#ifdef CGRAM_OFFSET
  x+=colstart;
  y+=rowstart;
#endif

  begin_tft_write();

  // No need to send x if it has not changed (speeds things up)
  if (addr_col != x) {
    DC_C; tft_Write_8(TFT_CASET);
    DC_D; tft_Write_32D(x);
    addr_col = x;
  }

  // No need to send y if it has not changed (speeds things up)
  if (addr_row != y) {
    DC_C; tft_Write_8(TFT_PASET);
    DC_D; tft_Write_32D(y);
    addr_row = y;
  }

  DC_C; tft_Write_8(TFT_RAMWR);
  DC_D; tft_Write_16(color);

  end_tft_write();
}



/***************************************************************************************
** Function name:           pushColor
** Description:             push a single pixel
***************************************************************************************/
void TFT_eSPI::pushColor(uint16_t color)
{
  begin_tft_write();

  tft_Write_16(color);

  end_tft_write();
}


/***************************************************************************************
** Function name:           pushColor
** Description:             push a single colour to "len" pixels
***************************************************************************************/
void TFT_eSPI::pushColor(uint16_t color, uint32_t len)
{
  begin_tft_write();

  pushBlock(color, len);

  end_tft_write();
}

/***************************************************************************************
** Function name:           startWrite
** Description:             begin transaction with CS low, MUST later call endWrite
***************************************************************************************/
void TFT_eSPI::startWrite(void)
{
  begin_tft_write();
  inTransaction = true;
}

/***************************************************************************************
** Function name:           endWrite
** Description:             end transaction with CS high
***************************************************************************************/
void TFT_eSPI::endWrite(void)
{
  inTransaction = false;
  DMA_BUSY_CHECK;         // Safety check - user code should have checked this!
  end_tft_write();
}

/***************************************************************************************
** Function name:           writeColor (use startWrite() and endWrite() before & after)
** Description:             raw write of "len" pixels avoiding transaction check
***************************************************************************************/
void TFT_eSPI::writeColor(uint16_t color, uint32_t len)
{
  pushBlock(color, len);
}

/***************************************************************************************
** Function name:           pushColors
** Description:             push an array of pixels for 16 bit raw image drawing
***************************************************************************************/
// Assumed that setAddrWindow() has previously been called

void TFT_eSPI::pushColors(uint8_t *data, uint32_t len)
{
  begin_tft_write();

  pushPixels(data, len>>1);

  end_tft_write();
}


/***************************************************************************************
** Function name:           pushColors
** Description:             push an array of pixels, for image drawing
***************************************************************************************/
void TFT_eSPI::pushColors(uint16_t *data, uint32_t len, bool swap)
{
  begin_tft_write();
  if (swap) {swap = _swapBytes; _swapBytes = true; }

  pushPixels(data, len);

  _swapBytes = swap; // Restore old value
  end_tft_write();
}


/***************************************************************************************
** Function name:           drawLine
** Description:             draw a line between 2 arbitrary points
***************************************************************************************/
// Bresenham's algorithm - thx wikipedia - speed enhanced by Bodmer to use
// an efficient FastH/V Line draw routine for line segments of 2 pixels or more
void TFT_eSPI::drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
  //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
  inTransaction = true;
  boolean steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    swap_coord(x0, y0);
    swap_coord(x1, y1);
  }

  if (x0 > x1) {
    swap_coord(x0, x1);
    swap_coord(y0, y1);
  }

  int32_t dx = x1 - x0, dy = abs(y1 - y0);;

  int32_t err = dx >> 1, ystep = -1, xs = x0, dlen = 0;

  if (y0 < y1) ystep = 1;

  // Split into steep and not steep for FastH/V separation
  if (steep) {
    for (; x0 <= x1; x0++) {
      dlen++;
      err -= dy;
      if (err < 0) {
        err += dx;
        if (dlen == 1) drawPixel(y0, xs, color);
        else drawFastVLine(y0, xs, dlen, color);
        dlen = 0; y0 += ystep; xs = x0 + 1;
      }
    }
    if (dlen) drawFastVLine(y0, xs, dlen, color);
  }
  else
  {
    for (; x0 <= x1; x0++) {
      dlen++;
      err -= dy;
      if (err < 0) {
        err += dx;
        if (dlen == 1) drawPixel(xs, y0, color);
        else drawFastHLine(xs, y0, dlen, color);
        dlen = 0; y0 += ystep; xs = x0 + 1;
      }
    }
    if (dlen) drawFastHLine(xs, y0, dlen, color);
  }
  inTransaction = false;
  end_tft_write();
}




/***************************************************************************************
** Function name:           drawFastVLine
** Description:             draw a vertical line
***************************************************************************************/
void TFT_eSPI::drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color)
{
  // Clipping
  if ((x < 0) || (x >= _width) || (y >= _height)) return;

  if (y < 0) { h += y; y = 0; }

  if ((y + h) > _height) h = _height - y;

  if (h < 1) return;

  begin_tft_write();

  setWindow(x, y, x, y + h - 1);

  pushBlock(color, h);

  end_tft_write();
}


/***************************************************************************************
** Function name:           drawFastHLine
** Description:             draw a horizontal line
***************************************************************************************/
void TFT_eSPI::drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color)
{
  // Clipping
  if ((y < 0) || (x >= _width) || (y >= _height)) return;

  if (x < 0) { w += x; x = 0; }

  if ((x + w) > _width)  w = _width  - x;

  if (w < 1) return;

  begin_tft_write();

  setWindow(x, y, x + w - 1, y);

  pushBlock(color, w);

  end_tft_write();
}


/***************************************************************************************
** Function name:           fillRect
** Description:             draw a filled rectangle
***************************************************************************************/
void TFT_eSPI::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)
{
  // Clipping
  if ((x >= _width) || (y >= _height)) return;

  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }

  if ((x + w) > _width)  w = _width  - x;
  if ((y + h) > _height) h = _height - y;

  if ((w < 1) || (h < 1)) return;

  begin_tft_write();

  setWindow(x, y, x + w - 1, y + h - 1);

  pushBlock(color, w * h);

  end_tft_write();
}


/***************************************************************************************
** Function name:           color565
** Description:             convert three 8 bit RGB levels to a 16 bit colour value
***************************************************************************************/
uint16_t TFT_eSPI::color565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}


/***************************************************************************************
** Function name:           color16to8
** Description:             convert 16 bit colour to an 8 bit 332 RGB colour value
***************************************************************************************/
uint8_t TFT_eSPI::color16to8(uint16_t c)
{
  return ((c & 0xE000)>>8) | ((c & 0x0700)>>6) | ((c & 0x0018)>>3);
}


/***************************************************************************************
** Function name:           color8to16
** Description:             convert 8 bit colour to a 16 bit 565 colour value
***************************************************************************************/
uint16_t TFT_eSPI::color8to16(uint8_t color)
{
  uint8_t  blue[] = {0, 11, 21, 31}; // blue 2 to 5 bit colour lookup table
  uint16_t color16 = 0;

  //        =====Green=====     ===============Red==============
  color16  = (color & 0x1C)<<6 | (color & 0xC0)<<5 | (color & 0xE0)<<8;
  //        =====Green=====    =======Blue======
  color16 |= (color & 0x1C)<<3 | blue[color & 0x03];

  return color16;
}

/***************************************************************************************
** Function name:           color16to24
** Description:             convert 16 bit colour to a 24 bit 888 colour value
***************************************************************************************/
uint32_t TFT_eSPI::color16to24(uint16_t color565)
{
  uint8_t r = (color565 >> 8) & 0xF8; r |= (r >> 5);
  uint8_t g = (color565 >> 3) & 0xFC; g |= (g >> 6);
  uint8_t b = (color565 << 3) & 0xF8; b |= (b >> 5);

  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | ((uint32_t)b << 0);
}

/***************************************************************************************
** Function name:           color24to16
** Description:             convert 24 bit colour to a 16 bit 565 colour value
***************************************************************************************/
uint32_t TFT_eSPI::color24to16(uint32_t color888)
{
  uint16_t r = (color888 >> 8) & 0xF800;
  uint16_t g = (color888 >> 5) & 0x07E0;
  uint16_t b = (color888 >> 3) & 0x001F;

  return (r | g | b);
}


/***************************************************************************************
** Function name:           invertDisplay
** Description:             invert the display colours i = 1 invert, i = 0 normal
***************************************************************************************/
void TFT_eSPI::invertDisplay(boolean i)
{
  begin_tft_write();
  // Send the command twice as otherwise it does not always work!
  writecommand(i ? TFT_INVON : TFT_INVOFF);
  writecommand(i ? TFT_INVON : TFT_INVOFF);
  end_tft_write();
}


/**************************************************************************
** Function name:           setAttribute
** Description:             Sets a control parameter of an attribute
**************************************************************************/
void TFT_eSPI::setAttribute(uint8_t attr_id, uint8_t param) {
    switch (attr_id) {
            break;
        case CP437_SWITCH:
            _cp437 = param;
            break;
        case UTF8_SWITCH:
            _utf8  = param;
            decoderState = 0;
            break;
        case PSRAM_ENABLE:
        //case 3: // TBD future feature control
        //    _tbd = param;
        //    break;
            if (psramFound()) _usePsram = param; // Enable the use of PSRAM (if available)
            else _usePsram = false;
        break;
    }

}


/**************************************************************************
** Function name:           getAttribute
** Description:             Get value of an attribute (control parameter)
**************************************************************************/
uint8_t TFT_eSPI::getAttribute(uint8_t attr_id) {
    switch (attr_id) {
        case CP437_SWITCH: // ON/OFF control of full CP437 character set
            return _cp437;
            break;
        case UTF8_SWITCH: // ON/OFF control of UTF-8 decoding
            return _utf8;
            break;
        //case 3: // TBD future feature control
        //    return _tbd;
        //    break;
        case PSRAM_ENABLE:
            return _usePsram;
        break;
    }

    return false;
}

/***************************************************************************************
** Function name:           decodeUTF8
** Description:             Serial UTF-8 decoder with fall-back to extended ASCII
*************************************************************************************x*/
#define DECODE_UTF8 // Test only, comment out to stop decoding
uint16_t TFT_eSPI::decodeUTF8(uint8_t c)
{
#ifdef DECODE_UTF8
  // 7 bit Unicode Code Point
  if ((c & 0x80) == 0x00) {
    decoderState = 0;
    return (uint16_t)c;
  }

  if (decoderState == 0)
  {
    // 11 bit Unicode Code Point
    if ((c & 0xE0) == 0xC0)
    {
      decoderBuffer = ((c & 0x1F)<<6);
      decoderState = 1;
      return 0;
    }

    // 16 bit Unicode Code Point
    if ((c & 0xF0) == 0xE0)
    {
      decoderBuffer = ((c & 0x0F)<<12);
      decoderState = 2;
      return 0;
    }
    // 21 bit Unicode  Code Point not supported so fall-back to extended ASCII
    //if ((c & 0xF8) == 0xF0) return (uint16_t)c;
  }
  else
  {
    if (decoderState == 2)
    {
      decoderBuffer |= ((c & 0x3F)<<6);
      decoderState--;
      return 0;
    }
    else
    {
      decoderBuffer |= (c & 0x3F);
      decoderState = 0;
      return decoderBuffer;
    }
  }

  decoderState = 0;
#endif

  return (uint16_t)c; // fall-back to extended ASCII
}


/***************************************************************************************
** Function name:           decodeUTF8
** Description:             Line buffer UTF-8 decoder with fall-back to extended ASCII
*************************************************************************************x*/
uint16_t TFT_eSPI::decodeUTF8(uint8_t *buf, uint16_t *index, uint16_t remaining)
{
  uint16_t c = buf[(*index)++];
  //Serial.print("Byte from string = 0x"); Serial.println(c, HEX);

#ifdef DECODE_UTF8
  // 7 bit Unicode
  if ((c & 0x80) == 0x00) return c;

  // 11 bit Unicode
  if (((c & 0xE0) == 0xC0) && (remaining > 1))
    return ((c & 0x1F)<<6) | (buf[(*index)++]&0x3F);

  // 16 bit Unicode
  if (((c & 0xF0) == 0xE0) && (remaining > 2))
  {
    c = ((c & 0x0F)<<12) | ((buf[(*index)++]&0x3F)<<6);
    return  c | ((buf[(*index)++]&0x3F));
  }

  // 21 bit Unicode not supported so fall-back to extended ASCII
  // if ((c & 0xF8) == 0xF0) return c;
#endif

  return c; // fall-back to extended ASCII
}


/***************************************************************************************
** Function name:           alphaBlend
** Description:             Blend 16bit foreground and background
*************************************************************************************x*/
uint16_t TFT_eSPI::alphaBlend(uint8_t alpha, uint16_t fgc, uint16_t bgc)
{
  // For speed use fixed point maths and rounding to permit a power of 2 division
  uint16_t fgR = ((fgc >> 10) & 0x3E) + 1;
  uint16_t fgG = ((fgc >>  4) & 0x7E) + 1;
  uint16_t fgB = ((fgc <<  1) & 0x3E) + 1;

  uint16_t bgR = ((bgc >> 10) & 0x3E) + 1;
  uint16_t bgG = ((bgc >>  4) & 0x7E) + 1;
  uint16_t bgB = ((bgc <<  1) & 0x3E) + 1;

  // Shift right 1 to drop rounding bit and shift right 8 to divide by 256
  uint16_t r = (((fgR * alpha) + (bgR * (255 - alpha))) >> 9);
  uint16_t g = (((fgG * alpha) + (bgG * (255 - alpha))) >> 9);
  uint16_t b = (((fgB * alpha) + (bgB * (255 - alpha))) >> 9);

  // Combine RGB565 colours into 16 bits
  //return ((r&0x18) << 11) | ((g&0x30) << 5) | ((b&0x18) << 0); // 2 bit greyscale
  //return ((r&0x1E) << 11) | ((g&0x3C) << 5) | ((b&0x1E) << 0); // 4 bit greyscale
  return (r << 11) | (g << 5) | (b << 0);
}

/***************************************************************************************
** Function name:           alphaBlend
** Description:             Blend 16bit foreground and background with dither
*************************************************************************************x*/
uint16_t TFT_eSPI::alphaBlend(uint8_t alpha, uint16_t fgc, uint16_t bgc, uint8_t dither)
{
  if (dither) {
    int16_t alphaDither = (int16_t)alpha - dither + random(2*dither+1); // +/-4 randomised
    alpha = (uint8_t)alphaDither;
    if (alphaDither <  0) alpha = 0;
    if (alphaDither >255) alpha = 255;
  }

  return alphaBlend(alpha, fgc, bgc);
}

/***************************************************************************************
** Function name:           alphaBlend
** Description:             Blend 24bit foreground and background with optional dither
*************************************************************************************x*/
uint32_t TFT_eSPI::alphaBlend24(uint8_t alpha, uint32_t fgc, uint32_t bgc, uint8_t dither)
{

  if (dither) {
    int16_t alphaDither = (int16_t)alpha - dither + random(2*dither+1); // +/-dither randomised
    alpha = (uint8_t)alphaDither;
    if (alphaDither <  0) alpha = 0;
    if (alphaDither >255) alpha = 255;
  }

  // For speed use fixed point maths and rounding to permit a power of 2 division
  uint16_t fgR = ((fgc >> 15) & 0x1FE) + 1;
  uint16_t fgG = ((fgc >>  7) & 0x1FE) + 1;
  uint16_t fgB = ((fgc <<  1) & 0x1FE) + 1;

  uint16_t bgR = ((bgc >> 15) & 0x1FE) + 1;
  uint16_t bgG = ((bgc >>  7) & 0x1FE) + 1;
  uint16_t bgB = ((bgc <<  1) & 0x1FE) + 1;

  // Shift right 1 to drop rounding bit and shift right 8 to divide by 256
  uint16_t r = (((fgR * alpha) + (bgR * (255 - alpha))) >> 9);
  uint16_t g = (((fgG * alpha) + (bgG * (255 - alpha))) >> 9);
  uint16_t b = (((fgB * alpha) + (bgB * (255 - alpha))) >> 9);

  // Combine RGB colours into 24 bits
  return (r << 16) | (g << 8) | (b << 0);
}


/***************************************************************************************
** Function name:           write
** Description:             draw characters piped through serial stream
***************************************************************************************/
size_t TFT_eSPI::write(uint8_t utf8)
{
  if (utf8 == '\r') return 1;

  uint16_t uniCode = utf8;

  if (_utf8) uniCode = decodeUTF8(utf8);

  if (uniCode == 0) return 1;

#ifdef SMOOTH_FONT
  if(fontLoaded) {
    //Serial.print("UniCode="); Serial.println(uniCode);
    //Serial.print("UTF8   ="); Serial.println(utf8);

    //fontFile = SPIFFS.open( _gFontFilename, "r" );

    //if(!fontFile)
    //{
    //  fontLoaded = false;
    //  return 1;
    //}

    drawGlyph(uniCode);

    //fontFile.close();
    return 1;
  }
#endif

  if (uniCode == '\n') uniCode+=22; // Make it a valid space character to stop errors
  else if (uniCode < 32) return 1;

  uint16_t width = 0;
  uint16_t height = 0;

//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv DEBUG vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
  //Serial.print((uint8_t) uniCode); // Debug line sends all printed TFT text to serial port
  //Serial.println(uniCode, HEX); // Debug line sends all printed TFT text to serial port
  //delay(5);                     // Debug optional wait for serial port to flush through
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ DEBUG ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
#ifdef LOAD_GFXFF
  if(!gfxFont) {
#endif
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

#ifdef LOAD_FONT2
  if (textfont == 2) {
    if (uniCode > 127) return 1;

    width = pgm_read_byte(widtbl_f16 + uniCode-32);
    height = chr_hgt_f16;
    // Font 2 is rendered in whole byte widths so we must allow for this
    width = (width + 6) / 8;  // Width in whole bytes for font 2, should be + 7 but must allow for font width change
    width = width * 8;        // Width converted back to pixels
  }
  #ifdef LOAD_RLE
  else
  #endif
#endif

#ifdef LOAD_RLE
  {
    if ((textfont>2) && (textfont<9)) {
      if (uniCode > 127) return 1;
      // Uses the fontinfo struct array to avoid lots of 'if' or 'switch' statements
      width = pgm_read_byte( (uint8_t *)pgm_read_dword( &(fontdata[textfont].widthtbl ) ) + uniCode-32 );
      height= pgm_read_byte( &fontdata[textfont].height );
    }
  }
#endif

#ifdef LOAD_GLCD
  if (textfont==1) {
      width =  6;
      height = 8;
  }
#else
  if (textfont==1) return 1;
#endif

  height = height * textsize;

  if (utf8 == '\n') {
    cursor_y += height;
    cursor_x  = 0;
  }
  else {
    if (textwrapX && (cursor_x + width * textsize > _width)) {
      cursor_y += height;
      cursor_x = 0;
    }
    if (textwrapY && (cursor_y >= (int32_t)_height)) cursor_y = 0;
    cursor_x += drawChar(uniCode, cursor_x, cursor_y, textfont);
  }

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
#ifdef LOAD_GFXFF
  } // Custom GFX font
  else {
    if(utf8 == '\n') {
      cursor_x  = 0;
      cursor_y += (int16_t)textsize *
                  (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
    }
    else {
      if (uniCode > pgm_read_word(&gfxFont->last )) return 1;
      if (uniCode < pgm_read_word(&gfxFont->first)) return 1;

      uint16_t   c2    = uniCode - pgm_read_word(&gfxFont->first);
      GFXglyph *glyph = &(((GFXglyph *)pgm_read_dword(&gfxFont->glyph))[c2]);
      uint8_t   w     = pgm_read_byte(&glyph->width),
                h     = pgm_read_byte(&glyph->height);
      if((w > 0) && (h > 0)) { // Is there an associated bitmap?
        int16_t xo = (int8_t)pgm_read_byte(&glyph->xOffset);
        if(textwrapX && ((cursor_x + textsize * (xo + w)) > _width)) {
          // Drawing character would go off right edge; wrap to new line
          cursor_x  = 0;
          cursor_y += (int16_t)textsize *
                      (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
        }
        if (textwrapY && (cursor_y >= (int32_t)_height)) cursor_y = 0;
        drawChar(cursor_x, cursor_y, uniCode, textcolor, textbgcolor, textsize);
      }
      cursor_x += pgm_read_byte(&glyph->xAdvance) * (int16_t)textsize;
    }
  }
#endif // LOAD_GFXFF
//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<

  return 1;
}


/***************************************************************************************
** Function name:           drawChar
** Description:             draw a Unicode glyph onto the screen
***************************************************************************************/
  // Any UTF-8 decoding must be done before calling drawChar()
int16_t TFT_eSPI::drawChar(uint16_t uniCode, int32_t x, int32_t y)
{
  return drawChar(uniCode, x, y, textfont);
}

  // Any UTF-8 decoding must be done before calling drawChar()
int16_t TFT_eSPI::drawChar(uint16_t uniCode, int32_t x, int32_t y, uint8_t font)
{
  if (!uniCode) return 0;

  if (font==1) {
#ifdef LOAD_GLCD
  #ifndef LOAD_GFXFF
    drawChar(x, y, uniCode, textcolor, textbgcolor, textsize);
    return 6 * textsize;
  #endif
#else
  #ifndef LOAD_GFXFF
    return 0;
  #endif
#endif

#ifdef LOAD_GFXFF
    drawChar(x, y, uniCode, textcolor, textbgcolor, textsize);
    if(!gfxFont) { // 'Classic' built-in font
    #ifdef LOAD_GLCD
      return 6 * textsize;
    #else
      return 0;
    #endif
    }
    else {
      if((uniCode >= pgm_read_word(&gfxFont->first)) && (uniCode <= pgm_read_word(&gfxFont->last) )) {
        uint16_t   c2    = uniCode - pgm_read_word(&gfxFont->first);
        GFXglyph *glyph = &(((GFXglyph *)pgm_read_dword(&gfxFont->glyph))[c2]);
        return pgm_read_byte(&glyph->xAdvance) * textsize;
      }
      else {
        return 0;
      }
    }
#endif
  }

  if ((font>1) && (font<9) && ((uniCode < 32) || (uniCode > 127))) return 0;

  int32_t width  = 0;
  int32_t height = 0;
  uint32_t flash_address = 0;
  uniCode -= 32;

#ifdef LOAD_FONT2
  if (font == 2) {
    flash_address = pgm_read_dword(&chrtbl_f16[uniCode]);
    width = pgm_read_byte(widtbl_f16 + uniCode);
    height = chr_hgt_f16;
  }
  #ifdef LOAD_RLE
  else
  #endif
#endif

#ifdef LOAD_RLE
  {
    if ((font>2) && (font<9)) {
      flash_address = pgm_read_dword( (const void*)(pgm_read_dword( &(fontdata[font].chartbl ) ) + uniCode*sizeof(void *)) );
      width = pgm_read_byte( (uint8_t *)pgm_read_dword( &(fontdata[font].widthtbl ) ) + uniCode );
      height= pgm_read_byte( &fontdata[font].height );
    }
  }
#endif

  int32_t w = width;
  int32_t pX      = 0;
  int32_t pY      = y;
  uint8_t line = 0;

#ifdef LOAD_FONT2 // chop out code if we do not need it
  if (font == 2) {
    w = w + 6; // Should be + 7 but we need to compensate for width increment
    w = w / 8;
    if (x + width * textsize >= (int16_t)_width) return width * textsize ;

    if (textcolor == textbgcolor || textsize != 1) {
      //begin_tft_write();          // Sprite class can use this function, avoiding begin_tft_write()
      inTransaction = true;

      for (int32_t i = 0; i < height; i++) {
        if (textcolor != textbgcolor) fillRect(x, pY, width * textsize, textsize, textbgcolor);

        for (int32_t k = 0; k < w; k++) {
          line = pgm_read_byte((uint8_t *)flash_address + w * i + k);
          if (line) {
            if (textsize == 1) {
              pX = x + k * 8;
              if (line & 0x80) drawPixel(pX, pY, textcolor);
              if (line & 0x40) drawPixel(pX + 1, pY, textcolor);
              if (line & 0x20) drawPixel(pX + 2, pY, textcolor);
              if (line & 0x10) drawPixel(pX + 3, pY, textcolor);
              if (line & 0x08) drawPixel(pX + 4, pY, textcolor);
              if (line & 0x04) drawPixel(pX + 5, pY, textcolor);
              if (line & 0x02) drawPixel(pX + 6, pY, textcolor);
              if (line & 0x01) drawPixel(pX + 7, pY, textcolor);
            }
            else {
              pX = x + k * 8 * textsize;
              if (line & 0x80) fillRect(pX, pY, textsize, textsize, textcolor);
              if (line & 0x40) fillRect(pX + textsize, pY, textsize, textsize, textcolor);
              if (line & 0x20) fillRect(pX + 2 * textsize, pY, textsize, textsize, textcolor);
              if (line & 0x10) fillRect(pX + 3 * textsize, pY, textsize, textsize, textcolor);
              if (line & 0x08) fillRect(pX + 4 * textsize, pY, textsize, textsize, textcolor);
              if (line & 0x04) fillRect(pX + 5 * textsize, pY, textsize, textsize, textcolor);
              if (line & 0x02) fillRect(pX + 6 * textsize, pY, textsize, textsize, textcolor);
              if (line & 0x01) fillRect(pX + 7 * textsize, pY, textsize, textsize, textcolor);
            }
          }
        }
        pY += textsize;
      }

      inTransaction = false;
      end_tft_write();
    }
    else { // Faster drawing of characters and background using block write
      begin_tft_write();

      setWindow(x, y, x + width - 1, y + height - 1);

      uint8_t mask;
      for (int32_t i = 0; i < height; i++) {
        pX = width;
        for (int32_t k = 0; k < w; k++) {
          line = pgm_read_byte((uint8_t *) (flash_address + w * i + k) );
          mask = 0x80;
          while (mask && pX) {
            if (line & mask) {tft_Write_16(textcolor);}
            else {tft_Write_16(textbgcolor);}
            pX--;
            mask = mask >> 1;
          }
        }
        if (pX) {tft_Write_16(textbgcolor);}
      }

      end_tft_write();
    }
  }

  #ifdef LOAD_RLE
  else
  #endif
#endif  //FONT2

#ifdef LOAD_RLE  //674 bytes of code
  // Font is not 2 and hence is RLE encoded
  {
    begin_tft_write();
    inTransaction = true;

    w *= height; // Now w is total number of pixels in the character
    if ((textsize != 1) || (textcolor == textbgcolor)) {
      if (textcolor != textbgcolor) fillRect(x, pY, width * textsize, textsize * height, textbgcolor);
      int32_t px = 0, py = pY; // To hold character block start and end column and row values
      int32_t pc = 0; // Pixel count
      uint8_t np = textsize * textsize; // Number of pixels in a drawn pixel

      uint8_t tnp = 0; // Temporary copy of np for while loop
      uint8_t ts = textsize - 1; // Temporary copy of textsize
      // 16 bit pixel count so maximum font size is equivalent to 180x180 pixels in area
      // w is total number of pixels to plot to fill character block
      while (pc < w) {
        line = pgm_read_byte((uint8_t *)flash_address);
        flash_address++;
        if (line & 0x80) {
          line &= 0x7F;
          line++;
          if (ts) {
            px = x + textsize * (pc % width); // Keep these px and py calculations outside the loop as they are slow
            py = y + textsize * (pc / width);
          }
          else {
            px = x + pc % width; // Keep these px and py calculations outside the loop as they are slow
            py = y + pc / width;
          }
          while (line--) { // In this case the while(line--) is faster
            pc++; // This is faster than putting pc+=line before while()?
            setWindow(px, py, px + ts, py + ts);

            if (ts) {
              tnp = np;
              while (tnp--) {tft_Write_16(textcolor);}
            }
            else {tft_Write_16(textcolor);}
            px += textsize;

            if (px >= (x + width * textsize)) {
              px = x;
              py += textsize;
            }
          }
        }
        else {
          line++;
          pc += line;
        }
      }
    }
    else { // Text colour != background && textsize = 1
           // so use faster drawing of characters and background using block write
      setWindow(x, y, x + width - 1, y + height - 1);

      // Maximum font size is equivalent to 180x180 pixels in area
      while (w > 0) {
        line = pgm_read_byte((uint8_t *)flash_address++); // 8 bytes smaller when incrementing here
        if (line & 0x80) {
          line &= 0x7F;
          line++; w -= line;
          pushBlock(textcolor,line);
        }
        else {
          line++; w -= line;
          pushBlock(textbgcolor,line);
        }
      }
    }
    inTransaction = false;
    end_tft_write();
  }
  // End of RLE font rendering
#endif
  return width * textsize;    // x +
}


/***************************************************************************************
** Function name:           drawString (with or without user defined font)
** Description :            draw string with padding if it is defined
***************************************************************************************/
// Without font number, uses font set by setTextFont()
int16_t TFT_eSPI::drawString(const String& string, int32_t poX, int32_t poY)
{
  int16_t len = string.length() + 2;
  char buffer[len];
  string.toCharArray(buffer, len);
  return drawString(buffer, poX, poY, textfont);
}
// With font number
int16_t TFT_eSPI::drawString(const String& string, int32_t poX, int32_t poY, uint8_t font)
{
  int16_t len = string.length() + 2;
  char buffer[len];
  string.toCharArray(buffer, len);
  return drawString(buffer, poX, poY, font);
}

// Without font number, uses font set by setTextFont()
int16_t TFT_eSPI::drawString(const char *string, int32_t poX, int32_t poY)
{
  return drawString(string, poX, poY, textfont);
}

// With font number. Note: font number is over-ridden if a smooth font is loaded
int16_t TFT_eSPI::drawString(const char *string, int32_t poX, int32_t poY, uint8_t font)
{
  int16_t sumX = 0;
  uint8_t padding = 1, baseline = 0;
  uint16_t cwidth = textWidth(string, font); // Find the pixel width of the string in the font
  uint16_t cheight = 8 * textsize;

#ifdef LOAD_GFXFF
  #ifdef SMOOTH_FONT
    bool freeFont = (font == 1 && gfxFont && !fontLoaded);
  #else
    bool freeFont = (font == 1 && gfxFont);
  #endif

  if (freeFont) {
    cheight = glyph_ab * textsize;
    poY += cheight; // Adjust for baseline datum of free fonts
    baseline = cheight;
    padding =101; // Different padding method used for Free Fonts

    // We need to make an adjustment for the bottom of the string (eg 'y' character)
    if ((textdatum == BL_DATUM) || (textdatum == BC_DATUM) || (textdatum == BR_DATUM)) {
      cheight += glyph_bb * textsize;
    }
  }
#endif


  // If it is not font 1 (GLCD or free font) get the baseline and pixel height of the font
#ifdef SMOOTH_FONT
  if(fontLoaded) {
    baseline = gFont.maxAscent;
    cheight  = fontHeight();
  }
  else
#endif
  if (font!=1) {
    baseline = pgm_read_byte( &fontdata[font].baseline ) * textsize;
    cheight = fontHeight(font);
  }

  if (textdatum || padX) {

    switch(textdatum) {
      case TC_DATUM:
        poX -= cwidth/2;
        padding += 1;
        break;
      case TR_DATUM:
        poX -= cwidth;
        padding += 2;
        break;
      case ML_DATUM:
        poY -= cheight/2;
        //padding += 0;
        break;
      case MC_DATUM:
        poX -= cwidth/2;
        poY -= cheight/2;
        padding += 1;
        break;
      case MR_DATUM:
        poX -= cwidth;
        poY -= cheight/2;
        padding += 2;
        break;
      case BL_DATUM:
        poY -= cheight;
        //padding += 0;
        break;
      case BC_DATUM:
        poX -= cwidth/2;
        poY -= cheight;
        padding += 1;
        break;
      case BR_DATUM:
        poX -= cwidth;
        poY -= cheight;
        padding += 2;
        break;
      case L_BASELINE:
        poY -= baseline;
        //padding += 0;
        break;
      case C_BASELINE:
        poX -= cwidth/2;
        poY -= baseline;
        padding += 1;
        break;
      case R_BASELINE:
        poX -= cwidth;
        poY -= baseline;
        padding += 2;
        break;
    }
    // Check coordinates are OK, adjust if not
    if (poX < 0) poX = 0;
    if (poX+cwidth > width())   poX = width() - cwidth;
    if (poY < 0) poY = 0;
    if (poY+cheight-baseline> height()) poY = height() - cheight;
  }


  int8_t xo = 0;
#ifdef LOAD_GFXFF
  if (freeFont && (textcolor!=textbgcolor)) {
      cheight = (glyph_ab + glyph_bb) * textsize;
      // Get the offset for the first character only to allow for negative offsets
      uint16_t c2 = 0;
      uint16_t len = strlen(string);
      uint16_t n = 0;

      while (n < len && c2 == 0) c2 = decodeUTF8((uint8_t*)string, &n, len - n);

      if((c2 >= pgm_read_word(&gfxFont->first)) && (c2 <= pgm_read_word(&gfxFont->last) )) {
        c2 -= pgm_read_word(&gfxFont->first);
        GFXglyph *glyph = &(((GFXglyph *)pgm_read_dword(&gfxFont->glyph))[c2]);
        xo = pgm_read_byte(&glyph->xOffset) * textsize;
        // Adjust for negative xOffset
        if (xo > 0) xo = 0;
        else cwidth -= xo;
        // Add 1 pixel of padding all round
        //cheight +=2;
        //fillRect(poX+xo-1, poY - 1 - glyph_ab * textsize, cwidth+2, cheight, textbgcolor);
        fillRect(poX+xo, poY - glyph_ab * textsize, cwidth, cheight, textbgcolor);
      }
      padding -=100;
    }
#endif

  uint16_t len = strlen(string);
  uint16_t n = 0;

#ifdef SMOOTH_FONT
  if(fontLoaded) {
    if (textcolor!=textbgcolor) fillRect(poX, poY, cwidth, cheight, textbgcolor);

    setCursor(poX, poY);

    while (n < len) {
      uint16_t uniCode = decodeUTF8((uint8_t*)string, &n, len - n);
      drawGlyph(uniCode);
    }
    sumX += cwidth;
    //fontFile.close();
  }
  else
#endif
  {
    while (n < len) {
      uint16_t uniCode = decodeUTF8((uint8_t*)string, &n, len - n);
      sumX += drawChar(uniCode, poX+sumX, poY, font);
    }
  }

//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv DEBUG vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// Switch on debugging for the padding areas
//#define PADDING_DEBUG

#ifndef PADDING_DEBUG
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ DEBUG ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

  if((padX>cwidth) && (textcolor!=textbgcolor)) {
    int16_t padXc = poX+cwidth+xo;
#ifdef LOAD_GFXFF
    if (freeFont) {
      poX +=xo; // Adjust for negative offset start character
      poY -= glyph_ab * textsize;
      sumX += poX;
    }
#endif
    switch(padding) {
      case 1:
        fillRect(padXc,poY,padX-cwidth,cheight, textbgcolor);
        break;
      case 2:
        fillRect(padXc,poY,(padX-cwidth)>>1,cheight, textbgcolor);
        padXc = (padX-cwidth)>>1;
        if (padXc>poX) padXc = poX;
        fillRect(poX - padXc,poY,(padX-cwidth)>>1,cheight, textbgcolor);
        break;
      case 3:
        if (padXc>padX) padXc = padX;
        fillRect(poX + cwidth - padXc,poY,padXc-cwidth,cheight, textbgcolor);
        break;
    }
  }


#else

//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv DEBUG vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// This is debug code to show text (green box) and blanked (white box) areas
// It shows that the padding areas are being correctly sized and positioned

  if((padX>sumX) && (textcolor!=textbgcolor)) {
    int16_t padXc = poX+sumX; // Maximum left side padding
#ifdef LOAD_GFXFF
    if ((font == 1) && (gfxFont)) poY -= glyph_ab;
#endif
    drawRect(poX,poY,sumX,cheight, TFT_GREEN);
    switch(padding) {
      case 1:
        drawRect(padXc,poY,padX-sumX,cheight, TFT_WHITE);
        break;
      case 2:
        drawRect(padXc,poY,(padX-sumX)>>1, cheight, TFT_WHITE);
        padXc = (padX-sumX)>>1;
        if (padXc>poX) padXc = poX;
        drawRect(poX - padXc,poY,(padX-sumX)>>1,cheight, TFT_WHITE);
        break;
      case 3:
        if (padXc>padX) padXc = padX;
        drawRect(poX + sumX - padXc,poY,padXc-sumX,cheight, TFT_WHITE);
        break;
    }
  }
#endif
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ DEBUG ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

return sumX;
}


/***************************************************************************************
** Function name:           drawCentreString (deprecated, use setTextDatum())
** Descriptions:            draw string centred on dX
***************************************************************************************/
int16_t TFT_eSPI::drawCentreString(const String& string, int32_t dX, int32_t poY, uint8_t font)
{
  int16_t len = string.length() + 2;
  char buffer[len];
  string.toCharArray(buffer, len);
  return drawCentreString(buffer, dX, poY, font);
}

int16_t TFT_eSPI::drawCentreString(const char *string, int32_t dX, int32_t poY, uint8_t font)
{
  uint8_t tempdatum = textdatum;
  int32_t sumX = 0;
  textdatum = TC_DATUM;
  sumX = drawString(string, dX, poY, font);
  textdatum = tempdatum;
  return sumX;
}


/***************************************************************************************
** Function name:           drawRightString (deprecated, use setTextDatum())
** Descriptions:            draw string right justified to dX
***************************************************************************************/
int16_t TFT_eSPI::drawRightString(const String& string, int32_t dX, int32_t poY, uint8_t font)
{
  int16_t len = string.length() + 2;
  char buffer[len];
  string.toCharArray(buffer, len);
  return drawRightString(buffer, dX, poY, font);
}

int16_t TFT_eSPI::drawRightString(const char *string, int32_t dX, int32_t poY, uint8_t font)
{
  uint8_t tempdatum = textdatum;
  int16_t sumX = 0;
  textdatum = TR_DATUM;
  sumX = drawString(string, dX, poY, font);
  textdatum = tempdatum;
  return sumX;
}


/***************************************************************************************
** Function name:           drawNumber
** Description:             draw a long integer
***************************************************************************************/
int16_t TFT_eSPI::drawNumber(long long_num, int32_t poX, int32_t poY)
{
  isDigits = true; // Eliminate jiggle in monospaced fonts
  char str[12];
  ltoa(long_num, str, 10);
  return drawString(str, poX, poY, textfont);
}

int16_t TFT_eSPI::drawNumber(long long_num, int32_t poX, int32_t poY, uint8_t font)
{
  isDigits = true; // Eliminate jiggle in monospaced fonts
  char str[12];
  ltoa(long_num, str, 10);
  return drawString(str, poX, poY, font);
}


/***************************************************************************************
** Function name:           drawFloat
** Descriptions:            drawFloat, prints 7 non zero digits maximum
***************************************************************************************/
// Assemble and print a string, this permits alignment relative to a datum
// looks complicated but much more compact and actually faster than using print class
int16_t TFT_eSPI::drawFloat(float floatNumber, uint8_t dp, int32_t poX, int32_t poY)
{
  return drawFloat(floatNumber, dp, poX, poY, textfont);
}

int16_t TFT_eSPI::drawFloat(float floatNumber, uint8_t dp, int32_t poX, int32_t poY, uint8_t font)
{
  isDigits = true;
  char str[14];               // Array to contain decimal string
  uint8_t ptr = 0;            // Initialise pointer for array
  int8_t  digits = 1;         // Count the digits to avoid array overflow
  float rounding = 0.5;       // Round up down delta

  if (dp > 7) dp = 7; // Limit the size of decimal portion

  // Adjust the rounding value
  for (uint8_t i = 0; i < dp; ++i) rounding /= 10.0;

  if (floatNumber < -rounding) {   // add sign, avoid adding - sign to 0.0!
    str[ptr++] = '-'; // Negative number
    str[ptr] = 0; // Put a null in the array as a precaution
    digits = 0;   // Set digits to 0 to compensate so pointer value can be used later
    floatNumber = -floatNumber; // Make positive
  }

  floatNumber += rounding; // Round up or down

  // For error put ... in string and return (all TFT_eSPI library fonts contain . character)
  if (floatNumber >= 2147483647) {
    strcpy(str, "...");
    return drawString(str, poX, poY, font);
  }
  // No chance of overflow from here on

  // Get integer part
  uint32_t temp = (uint32_t)floatNumber;

  // Put integer part into array
  ltoa(temp, str + ptr, 10);

  // Find out where the null is to get the digit count loaded
  while ((uint8_t)str[ptr] != 0) ptr++; // Move the pointer along
  digits += ptr;                  // Count the digits

  str[ptr++] = '.'; // Add decimal point
  str[ptr] = '0';   // Add a dummy zero
  str[ptr + 1] = 0; // Add a null but don't increment pointer so it can be overwritten

  // Get the decimal portion
  floatNumber = floatNumber - temp;

  // Get decimal digits one by one and put in array
  // Limit digit count so we don't get a false sense of resolution
  uint8_t i = 0;
  while ((i < dp) && (digits < 9)) { // while (i < dp) for no limit but array size must be increased
    i++;
    floatNumber *= 10;       // for the next decimal
    temp = floatNumber;      // get the decimal
    ltoa(temp, str + ptr, 10);
    ptr++; digits++;         // Increment pointer and digits count
    floatNumber -= temp;     // Remove that digit
  }

  // Finally we can plot the string and return pixel length
  return drawString(str, poX, poY, font);
}

/***************************************************************************************
** Function name:           setFreeFont
** Descriptions:            Sets the GFX free font to use
***************************************************************************************/

#ifdef LOAD_GFXFF

void TFT_eSPI::setFreeFont(const GFXfont *f)
{
  textfont = 1;
  gfxFont = (GFXfont *)f;

  glyph_ab = 0;
  glyph_bb = 0;
  uint16_t numChars = pgm_read_word(&gfxFont->last) - pgm_read_word(&gfxFont->first);

  // Find the biggest above and below baseline offsets
  for (uint8_t c = 0; c < numChars; c++)
  {
    GFXglyph *glyph1  = &(((GFXglyph *)pgm_read_dword(&gfxFont->glyph))[c]);
    int8_t ab = -pgm_read_byte(&glyph1->yOffset);
    if (ab > glyph_ab) glyph_ab = ab;
    int8_t bb = pgm_read_byte(&glyph1->height) - ab;
    if (bb > glyph_bb) glyph_bb = bb;
  }
}


/***************************************************************************************
** Function name:           setTextFont
** Description:             Set the font for the print stream
***************************************************************************************/
void TFT_eSPI::setTextFont(uint8_t f)
{
  textfont = (f > 0) ? f : 1; // Don't allow font 0
  gfxFont = NULL;
}

#else


/***************************************************************************************
** Function name:           setFreeFont
** Descriptions:            Sets the GFX free font to use
***************************************************************************************/

// Alternative to setTextFont() so we don't need two different named functions
void TFT_eSPI::setFreeFont(uint8_t font)
{
  setTextFont(font);
}


/***************************************************************************************
** Function name:           setTextFont
** Description:             Set the font for the print stream
***************************************************************************************/
void TFT_eSPI::setTextFont(uint8_t f)
{
  textfont = (f > 0) ? f : 1; // Don't allow font 0
}

#endif



/***************************************************************************************
** Function name:           getSPIinstance
** Description:             Get the instance of the SPI class (for ESP32 only)
***************************************************************************************/
#ifndef ESP32_PARALLEL
SPIClass& TFT_eSPI::getSPIinstance(void)
{
  return spi;
}
#endif


/***************************************************************************************
** Function name:           getSetup
** Description:             Get the setup details for diagnostic and sketch access
***************************************************************************************/
void TFT_eSPI::getSetup(setup_t &tft_settings)
{
#if defined (PROCESSOR_ID)
  tft_settings.esp = PROCESSOR_ID;
#else
  tft_settings.esp = -1;
#endif

#if defined (SUPPORT_TRANSACTIONS)
  tft_settings.trans = true;
#else
  tft_settings.trans = false;
#endif

#if defined (TFT_PARALLEL_8_BIT)
  tft_settings.serial = false;
  tft_settings.tft_spi_freq = 0;
#else
  tft_settings.serial = true;
  tft_settings.tft_spi_freq = SPI_FREQUENCY/100000;
  #ifdef SPI_READ_FREQUENCY
    tft_settings.tft_rd_freq = SPI_READ_FREQUENCY/100000;
  #endif
#endif

#if defined(TFT_SPI_OVERLAP)
  tft_settings.overlap = true;
#else
  tft_settings.overlap = false;
#endif

  tft_settings.tft_driver = TFT_DRIVER;
  tft_settings.tft_width  = _init_width;
  tft_settings.tft_height = _init_height;

#ifdef CGRAM_OFFSET
  tft_settings.r0_x_offset = colstart;
  tft_settings.r0_y_offset = rowstart;
  tft_settings.r1_x_offset = 0;
  tft_settings.r1_y_offset = 0;
  tft_settings.r2_x_offset = 0;
  tft_settings.r2_y_offset = 0;
  tft_settings.r3_x_offset = 0;
  tft_settings.r3_y_offset = 0;
#else
  tft_settings.r0_x_offset = 0;
  tft_settings.r0_y_offset = 0;
  tft_settings.r1_x_offset = 0;
  tft_settings.r1_y_offset = 0;
  tft_settings.r2_x_offset = 0;
  tft_settings.r2_y_offset = 0;
  tft_settings.r3_x_offset = 0;
  tft_settings.r3_y_offset = 0;
#endif

#if defined (TFT_MOSI)
  tft_settings.pin_tft_mosi = TFT_MOSI;
#else
  tft_settings.pin_tft_mosi = -1;
#endif

#if defined (TFT_MISO)
  tft_settings.pin_tft_miso = TFT_MISO;
#else
  tft_settings.pin_tft_miso = -1;
#endif

#if defined (TFT_SCLK)
  tft_settings.pin_tft_clk  = TFT_SCLK;
#else
  tft_settings.pin_tft_clk  = -1;
#endif

#if defined (TFT_CS)
  tft_settings.pin_tft_cs   = TFT_CS;
#else
  tft_settings.pin_tft_cs   = -1;
#endif

#if defined (TFT_DC)
  tft_settings.pin_tft_dc  = TFT_DC;
#else
  tft_settings.pin_tft_dc  = -1;
#endif

#if defined (TFT_RD)
  tft_settings.pin_tft_rd  = TFT_RD;
#else
  tft_settings.pin_tft_rd  = -1;
#endif

#if defined (TFT_WR)
  tft_settings.pin_tft_wr  = TFT_WR;
#else
  tft_settings.pin_tft_wr  = -1;
#endif

#if defined (TFT_RST)
  tft_settings.pin_tft_rst = TFT_RST;
#else
  tft_settings.pin_tft_rst = -1;
#endif

#if defined (TFT_PARALLEL_8_BIT)
  tft_settings.pin_tft_d0 = TFT_D0;
  tft_settings.pin_tft_d1 = TFT_D1;
  tft_settings.pin_tft_d2 = TFT_D2;
  tft_settings.pin_tft_d3 = TFT_D3;
  tft_settings.pin_tft_d4 = TFT_D4;
  tft_settings.pin_tft_d5 = TFT_D5;
  tft_settings.pin_tft_d6 = TFT_D6;
  tft_settings.pin_tft_d7 = TFT_D7;
#else
  tft_settings.pin_tft_d0 = -1;
  tft_settings.pin_tft_d1 = -1;
  tft_settings.pin_tft_d2 = -1;
  tft_settings.pin_tft_d3 = -1;
  tft_settings.pin_tft_d4 = -1;
  tft_settings.pin_tft_d5 = -1;
  tft_settings.pin_tft_d6 = -1;
  tft_settings.pin_tft_d7 = -1;
#endif

#if defined (TOUCH_CS)
  tft_settings.pin_tch_cs   = TOUCH_CS;
  tft_settings.tch_spi_freq = SPI_TOUCH_FREQUENCY/100000;
#else
  tft_settings.pin_tch_cs   = -1;
  tft_settings.tch_spi_freq = 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////
#ifdef TOUCH_CS
// The following touch screen support code by maxpautsch was merged 1/10/17
// https://github.com/maxpautsch

// Define TOUCH_CS is the user setup file to enable this code

// A demo is provided in examples Generic folder

// Additions by Bodmer to double sample, use Z value to improve detection reliability
// and to correct rotation handling

// See license in root directory.

/***************************************************************************************
** Function name:           getTouchRaw
** Description:             read raw touch position.  Always returns true.
***************************************************************************************/
uint8_t TFT_eSPI::getTouchRaw(uint16_t *x, uint16_t *y){
  uint16_t tmp;

  spi_begin_touch();

  // Start YP sample request for x position, read 4 times and keep last sample
  spi.transfer(0xd0);                    // Start new YP conversion
  spi.transfer(0);                       // Read first 8 bits
  spi.transfer(0xd0);                    // Read last 8 bits and start new YP conversion
  spi.transfer(0);                       // Read first 8 bits
  spi.transfer(0xd0);                    // Read last 8 bits and start new YP conversion
  spi.transfer(0);                       // Read first 8 bits
  spi.transfer(0xd0);                    // Read last 8 bits and start new YP conversion

  tmp = spi.transfer(0);                   // Read first 8 bits
  tmp = tmp <<5;
  tmp |= 0x1f & (spi.transfer(0x90)>>3);   // Read last 8 bits and start new XP conversion

  *x = tmp;

  // Start XP sample request for y position, read 4 times and keep last sample
  spi.transfer(0);                       // Read first 8 bits
  spi.transfer(0x90);                    // Read last 8 bits and start new XP conversion
  spi.transfer(0);                       // Read first 8 bits
  spi.transfer(0x90);                    // Read last 8 bits and start new XP conversion
  spi.transfer(0);                       // Read first 8 bits
  spi.transfer(0x90);                    // Read last 8 bits and start new XP conversion

  tmp = spi.transfer(0);                 // Read first 8 bits
  tmp = tmp <<5;
  tmp |= 0x1f & (spi.transfer(0)>>3);    // Read last 8 bits

  *y = tmp;

  spi_end_touch();

  return true;
}

/***************************************************************************************
** Function name:           getTouchRawZ
** Description:             read raw pressure on touchpad and return Z value.
***************************************************************************************/
uint16_t TFT_eSPI::getTouchRawZ(void){

  spi_begin_touch();

  // Z sample request
  int16_t tz = 0xFFF;
  spi.transfer(0xb0);               // Start new Z1 conversion
  tz += spi.transfer16(0xc0) >> 3;  // Read Z1 and start Z2 conversion
  tz -= spi.transfer16(0x00) >> 3;  // Read Z2

  spi_end_touch();

  return (uint16_t)tz;
}

/***************************************************************************************
** Function name:           validTouch
** Description:             read validated position. Return false if not pressed.
***************************************************************************************/
#define _RAWERR 20 // Deadband error allowed in successive position samples
uint8_t TFT_eSPI::validTouch(uint16_t *x, uint16_t *y, uint16_t threshold){
  uint16_t x_tmp, y_tmp, x_tmp2, y_tmp2;

  // Wait until pressure stops increasing to debounce pressure
  uint16_t z1 = 1;
  uint16_t z2 = 0;
  while (z1 > z2)
  {
    z2 = z1;
    z1 = getTouchRawZ();
    delay(1);
  }

  //  Serial.print("Z = ");Serial.println(z1);

  if (z1 <= threshold) return false;

  getTouchRaw(&x_tmp,&y_tmp);

  //  Serial.print("Sample 1 x,y = "); Serial.print(x_tmp);Serial.print(",");Serial.print(y_tmp);
  //  Serial.print(", Z = ");Serial.println(z1);

  delay(1); // Small delay to the next sample
  if (getTouchRawZ() <= threshold) return false;

  delay(2); // Small delay to the next sample
  getTouchRaw(&x_tmp2,&y_tmp2);

  //  Serial.print("Sample 2 x,y = "); Serial.print(x_tmp2);Serial.print(",");Serial.println(y_tmp2);
  //  Serial.print("Sample difference = ");Serial.print(abs(x_tmp - x_tmp2));Serial.print(",");Serial.println(abs(y_tmp - y_tmp2));

  if (abs(x_tmp - x_tmp2) > _RAWERR) return false;
  if (abs(y_tmp - y_tmp2) > _RAWERR) return false;

  *x = x_tmp;
  *y = y_tmp;

  return true;
}

/***************************************************************************************
** Function name:           getTouch
** Description:             read callibrated position. Return false if not pressed.
***************************************************************************************/
#define Z_THRESHOLD 350 // Touch pressure threshold for validating touches
uint8_t TFT_eSPI::getTouch(uint16_t *x, uint16_t *y, uint16_t threshold){
  uint16_t x_tmp, y_tmp;

  if (threshold<20) threshold = 20;
  if (_pressTime > millis()) threshold=20;

  uint8_t n = 5;
  uint8_t valid = 0;
  while (n--)
  {
    if (validTouch(&x_tmp, &y_tmp, threshold)) valid++;;
  }

  if (valid<1) { _pressTime = 0; return false; }

  _pressTime = millis() + 50;

  convertRawXY(&x_tmp, &y_tmp);

  if (x_tmp >= _width || y_tmp >= _height) return false;

  _pressX = x_tmp;
  _pressY = y_tmp;
  *x = _pressX;
  *y = _pressY;
  return valid;
}

/***************************************************************************************
** Function name:           convertRawXY
** Description:             convert raw touch x,y values to screen coordinates
***************************************************************************************/
void TFT_eSPI::convertRawXY(uint16_t *x, uint16_t *y)
{
  uint16_t x_tmp = *x, y_tmp = *y, xx, yy;

  if(!touchCalibration_rotate){
    xx=(x_tmp-touchCalibration_x0)*_width/touchCalibration_x1;
    yy=(y_tmp-touchCalibration_y0)*_height/touchCalibration_y1;
    if(touchCalibration_invert_x)
      xx = _width - xx;
    if(touchCalibration_invert_y)
      yy = _height - yy;
  } else {
    xx=(y_tmp-touchCalibration_x0)*_width/touchCalibration_x1;
    yy=(x_tmp-touchCalibration_y0)*_height/touchCalibration_y1;
    if(touchCalibration_invert_x)
      xx = _width - xx;
    if(touchCalibration_invert_y)
      yy = _height - yy;
  }
  *x = xx;
  *y = yy;
}

/***************************************************************************************
** Function name:           calibrateTouch
** Description:             generates calibration parameters for touchscreen.
***************************************************************************************/
void TFT_eSPI::calibrateTouch(uint16_t *parameters, uint32_t color_fg, uint32_t color_bg, uint8_t size){
  int16_t values[] = {0,0,0,0,0,0,0,0};
  uint16_t x_tmp, y_tmp;



  for(uint8_t i = 0; i<4; i++){
    fillRect(0, 0, size+1, size+1, color_bg);
    fillRect(0, _height-size-1, size+1, size+1, color_bg);
    fillRect(_width-size-1, 0, size+1, size+1, color_bg);
    fillRect(_width-size-1, _height-size-1, size+1, size+1, color_bg);

    if (i == 5) break; // used to clear the arrows

    switch (i) {
      case 0: // up left
        drawLine(0, 0, 0, size, color_fg);
        drawLine(0, 0, size, 0, color_fg);
        drawLine(0, 0, size , size, color_fg);
        break;
      case 1: // bot left
        drawLine(0, _height-size-1, 0, _height-1, color_fg);
        drawLine(0, _height-1, size, _height-1, color_fg);
        drawLine(size, _height-size-1, 0, _height-1 , color_fg);
        break;
      case 2: // up right
        drawLine(_width-size-1, 0, _width-1, 0, color_fg);
        drawLine(_width-size-1, size, _width-1, 0, color_fg);
        drawLine(_width-1, size, _width-1, 0, color_fg);
        break;
      case 3: // bot right
        drawLine(_width-size-1, _height-size-1, _width-1, _height-1, color_fg);
        drawLine(_width-1, _height-1-size, _width-1, _height-1, color_fg);
        drawLine(_width-1-size, _height-1, _width-1, _height-1, color_fg);
        break;
      }

    // user has to get the chance to release
    if(i>0) delay(1000);

    for(uint8_t j= 0; j<8; j++){
      // Use a lower detect threshold as corners tend to be less sensitive
      while(!validTouch(&x_tmp, &y_tmp, Z_THRESHOLD/2));
      values[i*2  ] += x_tmp;
      values[i*2+1] += y_tmp;
      }
    values[i*2  ] /= 8;
    values[i*2+1] /= 8;
  }


  // from case 0 to case 1, the y value changed.
  // If the measured delta of the touch x axis is bigger than the delta of the y axis, the touch and TFT axes are switched.
  touchCalibration_rotate = false;
  if(abs(values[0]-values[2]) > abs(values[1]-values[3])){
    touchCalibration_rotate = true;
    touchCalibration_x0 = (values[1] + values[3])/2; // calc min x
    touchCalibration_x1 = (values[5] + values[7])/2; // calc max x
    touchCalibration_y0 = (values[0] + values[4])/2; // calc min y
    touchCalibration_y1 = (values[2] + values[6])/2; // calc max y
  } else {
    touchCalibration_x0 = (values[0] + values[2])/2; // calc min x
    touchCalibration_x1 = (values[4] + values[6])/2; // calc max x
    touchCalibration_y0 = (values[1] + values[5])/2; // calc min y
    touchCalibration_y1 = (values[3] + values[7])/2; // calc max y
  }

  // in addition, the touch screen axis could be in the opposite direction of the TFT axis
  touchCalibration_invert_x = false;
  if(touchCalibration_x0 > touchCalibration_x1){
    values[0]=touchCalibration_x0;
    touchCalibration_x0 = touchCalibration_x1;
    touchCalibration_x1 = values[0];
    touchCalibration_invert_x = true;
  }
  touchCalibration_invert_y = false;
  if(touchCalibration_y0 > touchCalibration_y1){
    values[0]=touchCalibration_y0;
    touchCalibration_y0 = touchCalibration_y1;
    touchCalibration_y1 = values[0];
    touchCalibration_invert_y = true;
  }

  // pre calculate
  touchCalibration_x1 -= touchCalibration_x0;
  touchCalibration_y1 -= touchCalibration_y0;

  if(touchCalibration_x0 == 0) touchCalibration_x0 = 1;
  if(touchCalibration_x1 == 0) touchCalibration_x1 = 1;
  if(touchCalibration_y0 == 0) touchCalibration_y0 = 1;
  if(touchCalibration_y1 == 0) touchCalibration_y1 = 1;

  // export parameters, if pointer valid
  if(parameters != NULL){
    parameters[0] = touchCalibration_x0;
    parameters[1] = touchCalibration_x1;
    parameters[2] = touchCalibration_y0;
    parameters[3] = touchCalibration_y1;
    parameters[4] = touchCalibration_rotate | (touchCalibration_invert_x <<1) | (touchCalibration_invert_y <<2);
  }
}


/***************************************************************************************
** Function name:           setTouch
** Description:             imports calibration parameters for touchscreen.
***************************************************************************************/
void TFT_eSPI::setTouch(uint16_t *parameters){
  touchCalibration_x0 = parameters[0];
  touchCalibration_x1 = parameters[1];
  touchCalibration_y0 = parameters[2];
  touchCalibration_y1 = parameters[3];

  if(touchCalibration_x0 == 0) touchCalibration_x0 = 1;
  if(touchCalibration_x1 == 0) touchCalibration_x1 = 1;
  if(touchCalibration_y0 == 0) touchCalibration_y0 = 1;
  if(touchCalibration_y1 == 0) touchCalibration_y1 = 1;

  touchCalibration_rotate = parameters[4] & 0x01;
  touchCalibration_invert_x = parameters[4] & 0x02;
  touchCalibration_invert_y = parameters[4] & 0x04;
}

#endif

// for M5.Lcd comment out.
// #include "Extensions/Sprite.cpp"

#ifdef SMOOTH_FONT
 // Coded by Bodmer 10/2/18, see license in root directory.
 // This is part of the TFT_eSPI class and is associated with anti-aliased font functions


////////////////////////////////////////////////////////////////////////////////////////
// New anti-aliased (smoothed) font functions added below
////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************************
** Function name:           loadFont
** Description:             loads parameters from a font vlw array in memory
*************************************************************************************x*/
void TFT_eSPI::loadFont(const uint8_t array[])
{
  if (array == nullptr) return;
  fontPtr = (uint8_t*) array;
  loadFont("", false);
}

#ifdef FONT_FS_AVAILABLE
/***************************************************************************************
** Function name:           loadFont
** Description:             loads parameters from a font vlw file
*************************************************************************************x*/
void TFT_eSPI::loadFont(String fontName, fs::FS &ffs)
{
  fontFS = ffs;
  loadFont(fontName, false);
}
#endif

/***************************************************************************************
** Function name:           loadFont
** Description:             loads parameters from a font vlw file
*************************************************************************************x*/
void TFT_eSPI::loadFont(String fontName, bool flash)
{
  /*
    The vlw font format does not appear to be documented anywhere, so some reverse
    engineering has been applied!

    Header of vlw file comprises 6 uint32_t parameters (24 bytes total):
      1. The gCount (number of character glyphs)
      2. A version number (0xB = 11 for the one I am using)
      3. The font size (in points, not pixels)
      4. Deprecated mboxY parameter (typically set to 0)
      5. Ascent in pixels from baseline to top of "d"
      6. Descent in pixels from baseline to bottom of "p"

    Next are gCount sets of values for each glyph, each set comprises 7 int32t parameters (28 bytes):
      1. Glyph Unicode stored as a 32 bit value
      2. Height of bitmap bounding box
      3. Width of bitmap bounding box
      4. gxAdvance for cursor (setWidth in Processing)
      5. dY = distance from cursor baseline to top of glyph bitmap (signed value +ve = up)
      6. dX = distance from cursor to left side of glyph bitmap (signed value -ve = left)
      7. padding value, typically 0

    The bitmaps start next at 24 + (28 * gCount) bytes from the start of the file.
    Each pixel is 1 byte, an 8 bit Alpha value which represents the transparency from
    0xFF foreground colour, 0x00 background. The sketch uses a linear interpolation
    between the foreground and background RGB component colours. e.g.
        pixelRed = ((fgRed * alpha) + (bgRed * (255 - alpha))/255
    To gain a performance advantage fixed point arithmetic is used with rounding and
    division by 256 (shift right 8 bits is faster).

    After the bitmaps is:
       1 byte for font name string length (excludes null)
       a zero terminated character string giving the font name
       1 byte for Postscript name string length
       a zero/one terminated character string giving the font name
       last byte is 0 for non-anti-aliased and 1 for anti-aliased (smoothed)


    Glyph bitmap example is:
    // Cursor coordinate positions for this and next character are marked by 'C'
    // C<------- gxAdvance ------->C  gxAdvance is how far to move cursor for next glyph cursor position
    // |                           |
    // |                           |   ascent is top of "d", descent is bottom of "p"
    // +-- gdX --+             ascent
    // |         +-- gWidth--+     |   gdX is offset to left edge of glyph bitmap
    // |   +     x@.........@x  +  |   gdX may be negative e.g. italic "y" tail extending to left of
    // |   |     @@.........@@  |  |   cursor position, plot top left corner of bitmap at (cursorX + gdX)
    // |   |     @@.........@@ gdY |   gWidth and gHeight are glyph bitmap dimensions
    // |   |     .@@@.....@@@@  |  |
    // | gHeight ....@@@@@..@@  +  +    <-- baseline
    // |   |     ...........@@     |
    // |   |     ...........@@     |   gdY is the offset to the top edge of the bitmap
    // |   |     .@@.......@@. descent plot top edge of bitmap at (cursorY + yAdvance - gdY)
    // |   +     x..@@@@@@@..x     |   x marks the corner pixels of the bitmap
    // |                           |
    // +---------------------------+   yAdvance is y delta for the next line, font size or (ascent + descent)
    //                                 some fonts can overlay in y direction so may need a user adjust value

  */

  if (fontLoaded) unloadFont();

#ifdef FONT_FS_AVAILABLE
  if (fontName == "") fs_font = false;
  else { fontPtr = nullptr; fs_font = true; }

  if (fs_font) {
    spiffs = flash; // true if font is in SPIFFS

    if(spiffs) fontFS = SPIFFS;

    // Avoid a crash on the ESP32 if the file does not exist
    if (fontFS.exists("/" + fontName + ".vlw") == false) {
      Serial.println("Font file " + fontName + " not found!");
      return;
    }

    fontFile = fontFS.open( "/" + fontName + ".vlw", "r");

    if(!fontFile) return;

    fontFile.seek(0, fs::SeekSet);
  }
#endif

  gFont.gArray   = (const uint8_t*)fontPtr;

  gFont.gCount   = (uint16_t)readInt32(); // glyph count in file
                             readInt32(); // vlw encoder version - discard
  gFont.yAdvance = (uint16_t)readInt32(); // Font size in points, not pixels
                             readInt32(); // discard
  gFont.ascent   = (uint16_t)readInt32(); // top of "d"
  gFont.descent  = (uint16_t)readInt32(); // bottom of "p"

  // These next gFont values might be updated when the Metrics are fetched
  gFont.maxAscent  = gFont.ascent;   // Determined from metrics
  gFont.maxDescent = gFont.descent;  // Determined from metrics
  gFont.yAdvance   = gFont.ascent + gFont.descent;
  gFont.spaceWidth = gFont.yAdvance / 4;  // Guess at space width

  fontLoaded = true;

  // Fetch the metrics for each glyph
  loadMetrics();
}


/***************************************************************************************
** Function name:           loadMetrics
** Description:             Get the metrics for each glyph and store in RAM
*************************************************************************************x*/
//#define SHOW_ASCENT_DESCENT
void TFT_eSPI::loadMetrics(void)
{
  uint32_t headerPtr = 24;
  uint32_t bitmapPtr = headerPtr + gFont.gCount * 28;

#if defined (ESP32) && defined (CONFIG_SPIRAM_SUPPORT)
  if ( psramFound() )
  {
    gUnicode  = (uint16_t*)ps_malloc( gFont.gCount * 2); // Unicode 16 bit Basic Multilingual Plane (0-FFFF)
    gHeight   =  (uint8_t*)ps_malloc( gFont.gCount );    // Height of glyph
    gWidth    =  (uint8_t*)ps_malloc( gFont.gCount );    // Width of glyph
    gxAdvance =  (uint8_t*)ps_malloc( gFont.gCount );    // xAdvance - to move x cursor
    gdY       =  (int16_t*)ps_malloc( gFont.gCount * 2); // offset from bitmap top edge from lowest point in any character
    gdX       =   (int8_t*)ps_malloc( gFont.gCount );    // offset for bitmap left edge relative to cursor X
    gBitmap   = (uint32_t*)ps_malloc( gFont.gCount * 4); // seek pointer to glyph bitmap in the file
  }
  else
#endif
  {
    gUnicode  = (uint16_t*)malloc( gFont.gCount * 2); // Unicode 16 bit Basic Multilingual Plane (0-FFFF)
    gHeight   =  (uint8_t*)malloc( gFont.gCount );    // Height of glyph
    gWidth    =  (uint8_t*)malloc( gFont.gCount );    // Width of glyph
    gxAdvance =  (uint8_t*)malloc( gFont.gCount );    // xAdvance - to move x cursor
    gdY       =  (int16_t*)malloc( gFont.gCount * 2); // offset from bitmap top edge from lowest point in any character
    gdX       =   (int8_t*)malloc( gFont.gCount );    // offset for bitmap left edge relative to cursor X
    gBitmap   = (uint32_t*)malloc( gFont.gCount * 4); // seek pointer to glyph bitmap in the file
  }

#ifdef SHOW_ASCENT_DESCENT
  Serial.print("ascent  = "); Serial.println(gFont.ascent);
  Serial.print("descent = "); Serial.println(gFont.descent);
#endif

#ifdef FONT_FS_AVAILABLE
  if (fs_font) fontFile.seek(headerPtr, fs::SeekSet);
#endif

  uint16_t gNum = 0;

  while (gNum < gFont.gCount)
  {
    gUnicode[gNum]  = (uint16_t)readInt32(); // Unicode code point value
    gHeight[gNum]   =  (uint8_t)readInt32(); // Height of glyph
    gWidth[gNum]    =  (uint8_t)readInt32(); // Width of glyph
    gxAdvance[gNum] =  (uint8_t)readInt32(); // xAdvance - to move x cursor
    gdY[gNum]       =  (int16_t)readInt32(); // y delta from baseline
    gdX[gNum]       =   (int8_t)readInt32(); // x delta from cursor
    readInt32(); // ignored

    //Serial.print("Unicode = 0x"); Serial.print(gUnicode[gNum], HEX); Serial.print(", gHeight  = "); Serial.println(gHeight[gNum]);
    //Serial.print("Unicode = 0x"); Serial.print(gUnicode[gNum], HEX); Serial.print(", gWidth  = "); Serial.println(gWidth[gNum]);
    //Serial.print("Unicode = 0x"); Serial.print(gUnicode[gNum], HEX); Serial.print(", gxAdvance  = "); Serial.println(gxAdvance[gNum]);
    //Serial.print("Unicode = 0x"); Serial.print(gUnicode[gNum], HEX); Serial.print(", gdY  = "); Serial.println(gdY[gNum]);

    // Different glyph sets have different ascent values not always based on "d", so we could get
    // the maximum glyph ascent by checking all characters. BUT this method can generate bad values
    // for non-existant glyphs, so we will reply on processing for the value and disable this code for now...
    /*
    if (gdY[gNum] > gFont.maxAscent)
    {
      // Try to avoid UTF coding values and characters that tend to give duff values
      if (((gUnicode[gNum] > 0x20) && (gUnicode[gNum] < 0x7F)) || (gUnicode[gNum] > 0xA0))
      {
        gFont.maxAscent   = gdY[gNum];
#ifdef SHOW_ASCENT_DESCENT
        Serial.print("Unicode = 0x"); Serial.print(gUnicode[gNum], HEX); Serial.print(", maxAscent  = "); Serial.println(gFont.maxAscent);
#endif
      }
    }
    */

    // Different glyph sets have different descent values not always based on "p", so get maximum glyph descent
    if (((int16_t)gHeight[gNum] - (int16_t)gdY[gNum]) > gFont.maxDescent)
    {
      // Avoid UTF coding values and characters that tend to give duff values
      if (((gUnicode[gNum] > 0x20) && (gUnicode[gNum] < 0xA0) && (gUnicode[gNum] != 0x7F)) || (gUnicode[gNum] > 0xFF))
      {
        gFont.maxDescent   = gHeight[gNum] - gdY[gNum];
#ifdef SHOW_ASCENT_DESCENT
        Serial.print("Unicode = 0x"); Serial.print(gUnicode[gNum], HEX); Serial.print(", maxDescent = "); Serial.println(gHeight[gNum] - gdY[gNum]);
#endif
      }
    }

    gBitmap[gNum] = bitmapPtr;

    bitmapPtr += gWidth[gNum] * gHeight[gNum];

    gNum++;
    yield();
  }

  gFont.yAdvance = gFont.maxAscent + gFont.maxDescent;

  gFont.spaceWidth = (gFont.ascent + gFont.descent) * 2/7;  // Guess at space width
}


/***************************************************************************************
** Function name:           deleteMetrics
** Description:             Delete the old glyph metrics and free up the memory
*************************************************************************************x*/
void TFT_eSPI::unloadFont( void )
{
  if (gUnicode)
  {
    free(gUnicode);
    gUnicode = NULL;
  }

  if (gHeight)
  {
    free(gHeight);
    gHeight = NULL;
  }

  if (gWidth)
  {
    free(gWidth);
    gWidth = NULL;
  }

  if (gxAdvance)
  {
    free(gxAdvance);
    gxAdvance = NULL;
  }

  if (gdY)
  {
    free(gdY);
    gdY = NULL;
  }

  if (gdX)
  {
    free(gdX);
    gdX = NULL;
  }

  if (gBitmap)
  {
    free(gBitmap);
    gBitmap = NULL;
  }

  gFont.gArray = nullptr;

#ifdef FONT_FS_AVAILABLE
  if (fs_font && fontFile) fontFile.close();
#endif

  fontLoaded = false;
}


/***************************************************************************************
** Function name:           readInt32
** Description:             Get a 32 bit integer from the font file
*************************************************************************************x*/
uint32_t TFT_eSPI::readInt32(void)
{
  uint32_t val = 0;

#ifdef FONT_FS_AVAILABLE
  if (fs_font) {
    val |= fontFile.read() << 24;
    val |= fontFile.read() << 16;
    val |= fontFile.read() << 8;
    val |= fontFile.read();
  }
  else
#endif
  {
    val |= pgm_read_byte(fontPtr++) << 24;
    val |= pgm_read_byte(fontPtr++) << 16;
    val |= pgm_read_byte(fontPtr++) << 8;
    val |= pgm_read_byte(fontPtr++);
  }

  return val;
}


/***************************************************************************************
** Function name:           getUnicodeIndex
** Description:             Get the font file index of a Unicode character
*************************************************************************************x*/
bool TFT_eSPI::getUnicodeIndex(uint16_t unicode, uint16_t *index)
{
  for (uint16_t i = 0; i < gFont.gCount; i++)
  {
    if (gUnicode[i] == unicode)
    {
      *index = i;
      return true;
    }
  }
  return false;
}


/***************************************************************************************
** Function name:           drawGlyph
** Description:             Write a character to the TFT cursor position
*************************************************************************************x*/
// Expects file to be open
void TFT_eSPI::drawGlyph(uint16_t code)
{
  if (code < 0x21)
  {
    if (code == 0x20) {
      cursor_x += gFont.spaceWidth;
      return;
    }

    if (code == '\n') {
      cursor_x = 0;
      cursor_y += gFont.yAdvance;
      if (cursor_y >= _height) cursor_y = 0;
      return;
    }
  }

  uint16_t gNum = 0;
  bool found = getUnicodeIndex(code, &gNum);

  uint16_t fg = textcolor;
  uint16_t bg = textbgcolor;

  if (found)
  {

    if (textwrapX && (cursor_x + gWidth[gNum] + gdX[gNum] > _width))
    {
      cursor_y += gFont.yAdvance;
      cursor_x = 0;
    }
    if (textwrapY && ((cursor_y + gFont.yAdvance) >= _height)) cursor_y = 0;
    if (cursor_x == 0) cursor_x -= gdX[gNum];

    uint8_t* pbuffer = nullptr;
    const uint8_t* gPtr = (const uint8_t*) gFont.gArray;

#ifdef FONT_FS_AVAILABLE
    if (fs_font)
    {
      fontFile.seek(gBitmap[gNum], fs::SeekSet); // This is taking >30ms for a significant position shift
      pbuffer =  (uint8_t*)malloc(gWidth[gNum]);
    }
#endif

    int16_t  xs = 0;
    uint32_t dl = 0;
    uint8_t pixel;

    int16_t cy = cursor_y + gFont.maxAscent - gdY[gNum];
    int16_t cx = cursor_x + gdX[gNum];

    startWrite(); // Avoid slow ESP32 transaction overhead for every pixel

    for (int y = 0; y < gHeight[gNum]; y++)
    {
#ifdef FONT_FS_AVAILABLE
      if (fs_font) {
        if (spiffs)
        {
          fontFile.read(pbuffer, gWidth[gNum]);
          //Serial.println("SPIFFS");
        }
        else
        {
          endWrite();    // Release SPI for SD card transaction
          fontFile.read(pbuffer, gWidth[gNum]);
          startWrite();  // Re-start SPI for TFT transaction
          //Serial.println("Not SPIFFS");
        }
      }
#endif
      for (int x = 0; x < gWidth[gNum]; x++)
      {
#ifdef FONT_FS_AVAILABLE
        if (fs_font) pixel = pbuffer[x];
        else
#endif
        pixel = pgm_read_byte(gPtr + gBitmap[gNum] + x + gWidth[gNum] * y);

        if (pixel)
        {
          if (pixel != 0xFF)
          {
            if (dl) {
              if (dl==1) drawPixel(xs, y + cy, fg);
              else drawFastHLine( xs, y + cy, dl, fg);
              dl = 0;
            }
            if (getColor) bg = getColor(x + cx, y + cy);
            drawPixel(x + cx, y + cy, alphaBlend(pixel, fg, bg));
          }
          else
          {
            if (dl==0) xs = x + cx;
            dl++;
          }
        }
        else
        {
          if (dl) { drawFastHLine( xs, y + cy, dl, fg); dl = 0; }
        }
      }
      if (dl) { drawFastHLine( xs, y + cy, dl, fg); dl = 0; }
    }

    if (pbuffer) free(pbuffer);
    cursor_x += gxAdvance[gNum];
    endWrite();
  }
  else
  {
    // Not a Unicode in font so draw a rectangle and move on cursor
    drawRect(cursor_x, cursor_y + gFont.maxAscent - gFont.ascent, gFont.spaceWidth, gFont.ascent, fg);
    cursor_x += gFont.spaceWidth + 1;
  }
}

/***************************************************************************************
** Function name:           showFont
** Description:             Page through all characters in font, td ms between screens
*************************************************************************************x*/
void TFT_eSPI::showFont(uint32_t td)
{
  if(!fontLoaded) return;

  int16_t cursorX = width(); // Force start of new page to initialise cursor
  int16_t cursorY = height();// for the first character
  uint32_t timeDelay = 0;    // No delay before first page

  fillScreen(textbgcolor);

  for (uint16_t i = 0; i < gFont.gCount; i++)
  {
    // Check if this will need a new screen
    if (cursorX + gdX[i] + gWidth[i] >= width())  {
      cursorX = -gdX[i];

      cursorY += gFont.yAdvance;
      if (cursorY + gFont.maxAscent + gFont.descent >= height()) {
        cursorX = -gdX[i];
        cursorY = 0;
        delay(timeDelay);
        timeDelay = td;
        fillScreen(textbgcolor);
      }
    }

    setCursor(cursorX, cursorY);
    drawGlyph(gUnicode[i]);
    cursorX += gxAdvance[i];
    //cursorX +=  printToSprite( cursorX, cursorY, i );
    yield();
  }

  delay(timeDelay);
  fillScreen(textbgcolor);
  //fontFile.close();

}

#endif

////////////////////////////////////////////////////////////////////////////////////////
