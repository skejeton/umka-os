#pragma once
#include "vfs.h"
#include <stdarg.h>
#include <stddef.h>

struct RWStream {
  // optional fields, but useful
  size_t supposedoffs;
  size_t offs;
  size_t size;
  void *data;
  size_t (*write)(struct RWStream *s, const void *data, size_t size);
  size_t (*peek)(struct RWStream *s, void *dest, size_t size);
  size_t (*tell)(struct RWStream *s);
  size_t (*read)(struct RWStream *s, void *dest, size_t size);
} typedef RWStream;

RWStream rwStreamMkStr(char *str);
RWStream rwStreamMkStrS(char *str, size_t size);
RWStream rwStreamMkFile(Stream *stream);

unsigned long long fmtParseULL(RWStream *s, int base);
long long fmtParseLL(RWStream *s, int base);
double fmtParseD(RWStream *s);

size_t fmtvscanf(RWStream *s, const char *fmt, va_list ap);
size_t fmtscanf(RWStream *s, const char *fmt, ...);

void fmtStringifyULL(RWStream *s, unsigned long long n, int base, bool upper);
void fmtStringifyLL(RWStream *s, long long n, int base, bool upper, bool sign,
                    bool padsign);
void fmtStringifyD(RWStream *s, double n, bool upper, bool sign,
                   bool scientific, int prec, bool padsign);

size_t fmtvprintf(RWStream *s, const char *fmt, va_list ap);
size_t fmtprintf(RWStream *s, const char *fmt, ...);