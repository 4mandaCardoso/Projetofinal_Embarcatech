#include "ssd1306.h"
#include "font.h"

void ssd1306_init(ssd1306_t *ssd, uint8_t width, uint8_t height, bool external_vcc, uint8_t address, i2c_inst_t *i2c) {
  ssd->width = width;
  ssd->height = height;
  ssd->pages = height / 8U;
  ssd->address = address;
  ssd->i2c_port = i2c;
  ssd->bufsize = ssd->pages * ssd->width + 1;
  ssd->ram_buffer = calloc(ssd->bufsize, sizeof(uint8_t));
  ssd->ram_buffer[0] = 0x40;
  ssd->port_buffer[0] = 0x80;
}

void ssd1306_config(ssd1306_t *ssd) {
  ssd1306_command(ssd, SET_DISP | 0x00);
  ssd1306_command(ssd, SET_MEM_ADDR);
  ssd1306_command(ssd, 0x01);
  ssd1306_command(ssd, SET_DISP_START_LINE | 0x00);
  ssd1306_command(ssd, SET_SEG_REMAP | 0x01);
  ssd1306_command(ssd, SET_MUX_RATIO);
  ssd1306_command(ssd, HEIGHT - 1);
  ssd1306_command(ssd, SET_COM_OUT_DIR | 0x08);
  ssd1306_command(ssd, SET_DISP_OFFSET);
  ssd1306_command(ssd, 0x00);
  ssd1306_command(ssd, SET_COM_PIN_CFG);
  ssd1306_command(ssd, 0x12);
  ssd1306_command(ssd, SET_DISP_CLK_DIV);
  ssd1306_command(ssd, 0x80);
  ssd1306_command(ssd, SET_PRECHARGE);
  ssd1306_command(ssd, 0xF1);
  ssd1306_command(ssd, SET_VCOM_DESEL);
  ssd1306_command(ssd, 0x30);
  ssd1306_command(ssd, SET_CONTRAST);
  ssd1306_command(ssd, 0xFF);
  ssd1306_command(ssd, SET_ENTIRE_ON);
  ssd1306_command(ssd, SET_NORM_INV);
  ssd1306_command(ssd, SET_CHARGE_PUMP);
  ssd1306_command(ssd, 0x14);
  ssd1306_command(ssd, SET_DISP | 0x01);
}

void ssd1306_command(ssd1306_t *ssd, uint8_t command) {
  ssd->port_buffer[1] = command;
  i2c_write_blocking(
    ssd->i2c_port,
    ssd->address,
    ssd->port_buffer,
    2,
    false
  );
}

void ssd1306_send_data(ssd1306_t *ssd) {
  ssd1306_command(ssd, SET_COL_ADDR);
  ssd1306_command(ssd, 0);
  ssd1306_command(ssd, ssd->width - 1);
  ssd1306_command(ssd, SET_PAGE_ADDR);
  ssd1306_command(ssd, 0);
  ssd1306_command(ssd, ssd->pages - 1);
  i2c_write_blocking(
    ssd->i2c_port,
    ssd->address,
    ssd->ram_buffer,
    ssd->bufsize,
    false
  );
}

void ssd1306_pixel(ssd1306_t *ssd, uint8_t x, uint8_t y, bool value) {
  uint16_t index = (y >> 3) + (x << 3) + 1;
  uint8_t pixel = (y & 0b111);
  if (value)
    ssd->ram_buffer[index] |= (1 << pixel);
  else
    ssd->ram_buffer[index] &= ~(1 << pixel);
}

void ssd1306_fill(ssd1306_t *ssd, bool value) {
    for (uint8_t y = 0; y < ssd->height; ++y) {
        for (uint8_t x = 0; x < ssd->width; ++x) {
            ssd1306_pixel(ssd, x, y, value);
        }
    }
}

void ssd1306_rect(ssd1306_t *ssd, uint8_t top, uint8_t left, uint8_t width, uint8_t height, bool value, bool fill) {
  for (uint8_t x = left; x < left + width; ++x) {
    ssd1306_pixel(ssd, x, top, value);
    ssd1306_pixel(ssd, x, top + height - 1, value);
  }
  for (uint8_t y = top; y < top + height; ++y) {
    ssd1306_pixel(ssd, left, y, value);
    ssd1306_pixel(ssd, left + width - 1, y, value);
  }

  if (fill) {
    for (uint8_t x = left + 1; x < left + width - 1; ++x) {
      for (uint8_t y = top + 1; y < top + height - 1; ++y) {
        ssd1306_pixel(ssd, x, y, value);
      }
    }
  }
}

void ssd1306_line(ssd1306_t *ssd, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool value) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);

    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;

    int err = dx - dy;

    while (true) {
        ssd1306_pixel(ssd, x0, y0, value);

        if (x0 == x1 && y0 == y1) break;

        int e2 = err * 2;

        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }

        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void ssd1306_hline(ssd1306_t *ssd, uint8_t x0, uint8_t x1, uint8_t y, bool value) {
  for (uint8_t x = x0; x <= x1; ++x)
    ssd1306_pixel(ssd, x, y, value);
}

void ssd1306_vline(ssd1306_t *ssd, uint8_t x, uint8_t y0, uint8_t y1, bool value) {
  for (uint8_t y = y0; y <= y1; ++y)
    ssd1306_pixel(ssd, x, y, value);
}

void ssd1306_draw_char(ssd1306_t *ssd, char c, uint8_t x, uint8_t y) {
  uint16_t index = 0;
  if (c >= 'A' && c <= 'Z') {
    index = (c - 'A' + 11) * 8;
  }
  else if (c >= '0' && c <= '9') {
    index = (c - '0' + 1) * 8;
  }
  else if (c >= 'a' && c <= 'z') {
    index = (c - 'a' + 37) * 8;
  }
  else if (c == ':') {
    index = 63 * 8;
  }

  for (uint8_t i = 0; i < 8; ++i) {
    uint8_t line = font[index + i];
    for (uint8_t j = 0; j < 8; ++j) {
      ssd1306_pixel(ssd, x + i, y + j, line & (1 << j));
    }
  }
}

void ssd1306_draw_string(ssd1306_t *ssd, const char *str, uint8_t x, uint8_t y) {
  while (*str) {
    ssd1306_draw_char(ssd, *str++, x, y);
    x += 8;
    if (x + 8 >= ssd->width) {
      x = 0;
      y += 8;
    }
    if (y + 8 >= ssd->height) {
      break;
    }
  }
}

void ssd1306_init_config_clean(ssd1306_t *ssd,uint SCL,uint SDA,i2c_inst_t *PORT,uint8_t address) {
  i2c_init(PORT, 400 * 1000);
  gpio_set_function(SDA, GPIO_FUNC_I2C);
  gpio_set_function(SCL, GPIO_FUNC_I2C);
  gpio_pull_up(SDA);
  gpio_pull_up(SCL);

  ssd1306_init(ssd,WIDTH,HEIGHT,false,address,PORT);
  ssd1306_config(ssd);
  ssd1306_send_data(ssd);

  ssd1306_fill(ssd,false);
  ssd1306_send_data(ssd);
}

void ssd1306_select_edge(ssd1306_t *ssd,uint type,bool cor) {
  switch (type) {
    case 1:
      ssd1306_rect(ssd, 0,0, 127, 63, cor,!cor);
      break;
    case 2:
      ssd1306_rect(ssd, 3,3, 121, 57, cor,!cor);
      ssd1306_rect(ssd, 2,2, 123, 59, cor,!cor);
      ssd1306_rect(ssd, 1,1, 125, 61, cor,!cor);
      ssd1306_rect(ssd, 0,0, 127, 63, cor,!cor);
      break;
    case 3:
      ssd1306_rect(ssd, 0,0, 127, 63, cor,!cor);
      ssd1306_rect(ssd, 2,2, 123, 59, cor,!cor);
      ssd1306_rect(ssd, 4,4, 119, 55, cor,!cor);
      ssd1306_rect(ssd, 6,6, 115, 51, cor,!cor);
      break;
  }
}
