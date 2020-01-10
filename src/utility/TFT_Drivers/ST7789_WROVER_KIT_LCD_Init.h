// This is the command sequence that initialises the WROVER_KIT_LCD driver

// Configure WROVER_KIT_LCD display


{


  /**********************************/
  spi_end();

  spi_begin_read();
  DC_C;
  tft_Write_8(0xD9);
  DC_D;
  spi.write(0x10);
  DC_C;
  tft_Write_8( ST7789_RDDID );
  DC_D;
  _id = tft_Read_8() << 16;
  _id |= tft_Read_8() << 8;
  _id |= tft_Read_8();
  CS_H;
  spi_end_read();
  spi_end();
  delay(120);
  log_d("Display ID: 0x%06X", _id);

  if( _id == 0 ) {
    // Software reset, needed by ILI9341 after the previous query
    spi_begin();
    writecommand(TFT_SWRST);
    spi_end();
    delay(150); // Wait for reset to complete
  }

  // resume init
  spi_begin();
  /**********************************/


  if( _id > 0 ) {
    log_d("ST7789 Init");
    static const uint8_t PROGMEM st7789[] = {
      16,
      TFT_SLPOUT, 1, 255,
      TFT_COLMOD, 2, 0x55, 10,
      TFT_MADCTL, 1, 0x00,
      0x3a, 1, 0x55,
      0xb2, 5, 0x0c,0x0c,0x00,0x33,0x33,
      0xb7, 1, 0x35,
      0xbb, 1, 0x2b,
      0xc0, 1, 0x2c,
      0xc2, 2, 0x01,0xff,
      0xc3, 1, 0x11,
      0xc4, 1, 0x20,
      0xc6, 1, 0x0f,
      0xd0, 2, 0xa4,0xa1,
      0xe0, 14, 0xd0,0x00,0x05,0x0e,0x15,0x0d,0x37,0x43,0x47,0x09,0x15,0x12,0x16,0x19,
      0xe1, 14, 0xd0,0x00,0x05,0x0d,0x0c,0x06,0x2d,0x44,0x40,0x0e,0x1c,0x18,0x16,0x19,
      TFT_DISPON, 1, 255,
    };
    commandList(st7789);
  } else {
    log_d("ILI9341 Init");
    static const uint8_t PROGMEM ili9341[] = {
      21,
      0xef, 3, 0x03,0x80,0x02,
      0xcf, 3, 0x00,0xc1,0x30,
      0xed, 4, 0x64,0x03,0x12,0x81,
      0xe8, 3, 0x85,0x00,0x78,
      0xcb, 5, 0x39,0x2c,0x00,0x34,0x02,
      0xf7, 1, 0x20,
      0xea, 2, 0x00,0x00,
      0xc0, 1, 0x23,
      0xc1, 1, 0x10,
      0xc5, 2, 0x3e,0x28,
      0xc7, 1, 0x86,
      0x36, 1, 0xa8, // TFT_MADCTL
      0x3a, 1, 0x55,
      0xb1, 2, 0x00,0x18,
      0xb6, 3, 0x08,0x82,0x27,
      0xf2, 1, 0x00,
      0x26, 1, 0x01,
      0xe0, 15, 0x0f,0x31,0x2b,0x0c,0x0e,0x08,0x4e,0xf1,0x37,0x07,0x10,0x03,0x0e,0x09,0x00,
      0xe1, 15, 0x00,0x0e,0x14,0x03,0x11,0x07,0x31,0xc1,0x48,0x08,0x0f,0x0c,0x31,0x36,0x0f,
      TFT_SLPOUT, 1, 255,
      TFT_DISPON, 1, 255,
    };
    commandList(ili9341);

  }
}
// End of WROVER_KIT_LCD display configuration