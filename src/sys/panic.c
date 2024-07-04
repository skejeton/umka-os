#include "gfx.h"
#include <stdio.h>

void panic(const char *msg, ...) {
  asm("cli");

  char buf[4096];
  va_list args;
  va_start(args, msg);
  buf[0] = 'O';
  buf[1] = 'o';
  buf[2] = 'p';
  buf[3] = 's';
  buf[4] = '!';
  buf[5] = '\n';
  vsnprintf(buf + 6, 4090, msg, args);
  va_end(args);

  int col = 0;
  int row = 0;
  int w, h;
  gfxDims(&w, &h);

  gfxSetColor(0, 0, 0, 255);
  gfxDrawRect(0, 0, w, h);

  gfxSetColor(255, 0, 0, 255);
  for (int i = 0; buf[i]; i++) {
    if (buf[i] == '\n') {
      col = 0;
      row++;
    } else {
      gfxDrawChar(col * 8, row * 10, buf[i]);
      col++;
    }
  }

  gfxSwap();

  asm("hlt");
}