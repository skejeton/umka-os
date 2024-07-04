#define QOI_IMPLEMENTATION
#include "qoi.h"
#include <stdint.h>
#include <stdlib.h>

struct {
  char whatever1[18];
  uint16_t width;
  uint16_t height;
  char whatever2[18];
  uint8_t *pixels;
} __attribute__((packed)) typedef VesaModeInfo;

struct {
  uint32_t width, height;
  uint8_t *pixels;
} typedef VesaBlitInfo;

extern VesaModeInfo vesa_mode_info_struct;

VesaBlitInfo vesaLoadBlitInfo() {
  return (VesaBlitInfo){vesa_mode_info_struct.width,
                        vesa_mode_info_struct.height,
                        vesa_mode_info_struct.pixels};
}

#define FNV_32_INIT ((uint32_t)0x811c9dc5)
uint32_t fnv32buf(void *buf, size_t len, uint32_t hval) {
  unsigned char *bp = (unsigned char *)buf;
  unsigned char *be = bp + len;
  while (bp < be) {
    hval ^= (uint32_t)*bp++;
    hval +=
        (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval << 24);
  }
  return hval;
}

#define CELL_SIZE 16
#define CELLS_X 64
#define CELLS_Y 48

uint32_t hashes1[CELLS_X * CELLS_Y];
uint32_t hashes2[CELLS_X * CELLS_Y];

enum {
  CMD_DRAW_BCKG = 0,
  CMD_DRAW_ICON = 1,
  CMD_DRAW_CHAR = 2,
  CMD_DRAW_RECT = 3,
  CMD_DRAW_CLIP = 4
};

struct Command {
  int type;
  char data;
  uint32_t color;
  int x, y, w, h;
};

static struct Command *commands;
static int command_count = 0;

//------------------------------------------------------------------------------

static qoi_desc icon_desc;
static uint32_t *icon_data;
static qoi_desc background_desc;
static uint32_t *background_data;
static uint32_t *backbuffer;
static uint32_t color = 0xFFFFFF;
static int cx, cy, cw, ch;

void gfxInit() {
  commands = malloc(4096 * sizeof(struct Command));

  backbuffer =
      malloc(vesa_mode_info_struct.width * vesa_mode_info_struct.height * 4);

  memset(hashes1, 0, sizeof(hashes1));
  memset(hashes2, 1, sizeof(hashes2));

  icon_data = qoi_read("res/icons.qoi", &icon_desc, 0);
  background_data = qoi_read("res/background.qoi", &background_desc, 0);

  // Swap red and blue
  for (int i = 0; i < background_desc.width * background_desc.height; i++) {
    uint32_t c = background_data[i];
    background_data[i] =
        (c & 0xFF00FF00) | ((c & 0xFF) << 16) | ((c & 0xFF0000) >> 16);
  }
}

void rectIntersect(int x1, int y1, int w1, int h1, int x2, int y2, int w2,
                   int h2, int *x, int *y, int *w, int *h) {
  *x = x1 > x2 ? x1 : x2;
  *y = y1 > y2 ? y1 : y2;
  *w = (x1 + w1 < x2 + w2 ? x1 + w1 : x2 + w2) - *x;
  *h = (y1 + h1 < y2 + h2 ? y1 + h1 : y2 + h2) - *y;
}

void gfxSetPixel(int x, int y, uint32_t color) {
  if (x < cx || y < cy || x >= (cx + cw) || y >= (cy + ch))
    return;

  backbuffer[x + y * vesa_mode_info_struct.width] = color;
}

void gfxDoDrawIcon(int px, int py, char c) {
  for (int x = 0; x < 12; x++) {
    for (int y = 0; y < 16; y++) {
      int ix = x + c * 12;
      int iy = y;

      uint32_t ccolor = icon_data[ix + iy * icon_desc.width];
      if (ccolor & 0xFF)
        gfxSetPixel(px + x, py + y, ccolor & color);
    }
  }
}

extern char font8x8_basic[128][8];
void gfxDoDrawChar(int x, int y, char c) {
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      if (font8x8_basic[c][i] & (1 << j)) {
        gfxSetPixel(x + j, y + i, color);
      }
    }
  }
}

void gfxDoDrawRect(int xs, int ys, int ws, int hs) {
  int x, y, w, h;

  rectIntersect(cx, cy, cw, ch, xs, ys, ws, hs, &x, &y, &w, &h);

  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      backbuffer[(x + j) + (y + i) * vesa_mode_info_struct.width] = color;
    }
  }
}

bool rectOverlap(int x1, int y1, int w1, int h1, int x2, int y2, int w2,
                 int h2) {
  return x1 < x2 + w2 && x1 + w1 > x2 && y1 < y2 + h2 && y1 + h1 > y2;
}

void gfxRedrawRegion(int x, int y, int w, int h) {
  cx = 0;
  cy = 0;
  cw = vesa_mode_info_struct.width;
  ch = vesa_mode_info_struct.height;

  for (int i = 0; i < command_count; i++) {
    struct Command cmd = commands[i];
    if (!rectOverlap(x, y, w, h, cmd.x, cmd.y, cmd.w, cmd.h))
      continue;
    color = cmd.color;
    int x1, y1, w1, h1;
    switch (cmd.type) {
    case CMD_DRAW_BCKG:
      rectIntersect(x, y, w, h, cmd.x, cmd.y, cmd.w, cmd.h, &x1, &y1, &w1, &h1);

      for (int y = 0; y < h1; y++) {
        int texY = y + y1 - cmd.y;
        if (texY < 0 || texY >= background_desc.height)
          continue;

        memcpy(backbuffer + (x1 + (y1 + y) * vesa_mode_info_struct.width),
               background_data + ((x1 - cmd.x) + texY * background_desc.width),
               w1 * 4);
      }
      break;
    case CMD_DRAW_ICON:
      gfxDoDrawIcon(cmd.x, cmd.y, cmd.data);
      break;
    case CMD_DRAW_CHAR:
      gfxDoDrawChar(cmd.x, cmd.y, cmd.data);
      break;
    case CMD_DRAW_RECT:
      rectIntersect(x, y, w, h, cmd.x, cmd.y, cmd.w, cmd.h, &x1, &y1, &w1, &h1);
      gfxDoDrawRect(x1, y1, w1, h1);
      break;
    case CMD_DRAW_CLIP:
      cx = cmd.x;
      cy = cmd.y;
      cw = cmd.w;
      ch = cmd.h;
      break;
    }
  }

  int px = x;
  int py = y;

  // copy
  VesaBlitInfo vesa_blit_info = vesaLoadBlitInfo();
  for (int y = py; y < py + h; y++) {
    for (int x = px; x < px + w; x++) {
      vesa_blit_info.pixels[(x + y * vesa_blit_info.width) * 3] =
          backbuffer[x + y * vesa_blit_info.width];
      vesa_blit_info.pixels[(x + y * vesa_blit_info.width) * 3 + 1] =
          backbuffer[x + y * vesa_blit_info.width] >> 8;
      vesa_blit_info.pixels[(x + y * vesa_blit_info.width) * 3 + 2] =
          backbuffer[x + y * vesa_blit_info.width] >> 16;
    }
  }
}

void panic(const char *, ...);
extern bool gfxEnable;

void gfxSwap() {
  for (int i = 0; i < command_count; i++) {
    uint32_t cmdhash = FNV_32_INIT;
    struct Command cmd = commands[i];
    cmdhash = fnv32buf(&cmd, sizeof(cmd), FNV_32_INIT);

    int x1 = cmd.x / CELL_SIZE;
    int y1 = cmd.y / CELL_SIZE;
    int x2 = (cmd.x + cmd.w) / CELL_SIZE;
    int y2 = (cmd.y + cmd.h) / CELL_SIZE;

    if (x1 < 0)
      x1 = 0;
    if (y1 < 0)
      y1 = 0;
    if (x1 >= CELLS_X)
      x1 = CELLS_X - 1;
    if (y1 >= CELLS_Y)
      y1 = CELLS_Y - 1;

    if (x2 < 0)
      x2 = 0;
    if (y2 < 0)
      y2 = 0;
    if (x2 >= CELLS_X)
      x2 = CELLS_X - 1;
    if (y2 >= CELLS_Y)
      y2 = CELLS_Y - 1;

    for (int x = x1; x <= x2; x++) {
      for (int y = y1; y <= y2; y++) {
        hashes1[x + y * CELLS_X] =
            fnv32buf(&cmdhash, sizeof(cmdhash), hashes1[x + y * CELLS_X]);
      }
    }
  }

  for (int y = 0; y < CELLS_Y; y++) {
    for (int x = 0; x < CELLS_X; x++) {
      int start = x;
      while (x < CELLS_X &&
             hashes1[x + y * CELLS_X] != hashes2[x + y * CELLS_X])
        x++;

      if (start != x) {
        gfxRedrawRegion(start * CELL_SIZE, y * CELL_SIZE,
                        CELL_SIZE * (x - start), CELL_SIZE);
      }
    }
  }

  command_count = 0;

  for (int i = 0; i < CELLS_X * CELLS_Y; i++) {
    hashes2[i] = hashes1[i];
    hashes1[i] = FNV_32_INIT;
  }
}

void gfxDrawBackground() {
  if (command_count >= 4096) {
    return;
  }

  commands[command_count++] = (struct Command){
      CMD_DRAW_BCKG,
      0,
      0xFFFFFFFF,
      (vesa_mode_info_struct.width - background_desc.width) / 2,
      (vesa_mode_info_struct.height - background_desc.height) / 2,
      background_desc.width,
      background_desc.height};
}

void gfxSetColor(int r, int g, int b, int a) {
  color = (r << 16) | (g << 8) | b;
}

void gfxDrawIcon(int px, int py, char c) {
  if (command_count >= 4096) {
    return;
  }

  commands[command_count++] = (struct Command){
      CMD_DRAW_ICON, c, color, px, py, 12, 16,
  };
}

void gfxDrawChar(int x, int y, char c) {
  if (command_count >= 4096) {
    return;
  }

  commands[command_count++] = (struct Command){
      CMD_DRAW_CHAR, c, color, x, y, 8, 8,
  };
}

void gfxDrawRect(int x, int y, int w, int h) {
  if (command_count >= 4096) {
    return;
  }

  commands[command_count++] = (struct Command){
      CMD_DRAW_RECT, 0, color, x, y, w, h,
  };
}

void gfxDrawClip(int x, int y, int w, int h) {
  if (command_count >= 4096) {
    return;
  }

  int x1, y1, w1, h1;
  rectIntersect(x, y, w, h, 0, 0, vesa_mode_info_struct.width,
                vesa_mode_info_struct.height, &x1, &y1, &w1, &h1);

  commands[command_count++] = (struct Command){
      CMD_DRAW_CLIP, 0, 0, x1, y1, w1, h1,
  };
}

void gfxDims(int *w, int *h) {
  *w = vesa_mode_info_struct.width;
  *h = vesa_mode_info_struct.height;
}
