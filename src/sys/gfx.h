#pragma once

void gfxInit();
void gfxSwap();

void gfxSetColor(int r, int g, int b, int a);

void gfxDrawBackground();
void gfxDrawIcon(int x, int y, char c);
void gfxDrawChar(int x, int y, char c);
void gfxDrawRect(int x, int y, int w, int h);
void gfxDrawClip(int x, int y, int w, int h);
void gfxDims(int *w, int *h);