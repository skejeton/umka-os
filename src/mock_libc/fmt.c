#include "fmt.h"
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

size_t _vfsWrite(RWStream *stream, const void *data, size_t size) {
  stream->supposedoffs += size;

  return vfsWrite((Stream *)stream->data, data, size);
}

size_t _vfsPeek(RWStream *stream, void *dest, size_t size) {
  size_t at = vfsTell((Stream *)stream->data);
  size_t wrote = vfsRead((Stream *)stream->data, dest, size);
  vfsSeek((Stream *)stream->data, at, 0);
  return wrote;
}

size_t _vfsTell(RWStream *stream) { return vfsTell((Stream *)stream->data); }

size_t _vfsRead(RWStream *stream, void *dest, size_t size) {
  return vfsRead((Stream *)stream->data, dest, size);
}

size_t _strWrite(RWStream *stream, const void *data_, size_t size) {
  stream->supposedoffs += size;

  if (stream->size == 0) {
    return 0;
  }

  size_t written = 0;
  const char *data = (const char *)data_;
  char *str = (char *)stream->data;
  while (size && stream->offs < stream->size - 1) {
    str[stream->offs++] = *data++;
    size--;
    written++;
  }
  str[stream->offs] = 0;
  return written;
}

size_t _strPeek(RWStream *stream, void *dest_, size_t size) {
  size_t written = 0;
  char *dest = dest_;
  const char *str = (const char *)stream->data + stream->offs;
  while (size && *str) {
    *dest++ = *str++;
    size--;
    written++;
  }
  return written;
}

size_t _strTell(RWStream *stream) { return stream->offs; }

size_t _strRead(RWStream *stream, void *dest_, size_t size) {
  size_t written = 0;
  char *dest = dest_;
  const char *str = (const char *)stream->data;
  while (size && stream->offs < stream->size && str[stream->offs]) {
    *dest++ = str[stream->offs++];
    size--;
    written++;
  }
  return written;
}

RWStream rwStreamMkStr(char *str) {
  RWStream rw = {0};
  rw.size = 1 << 31;
  rw.offs = 0;
  rw.data = (void *)str;
  rw.write = _strWrite;
  rw.peek = _strPeek;
  rw.tell = _strTell;
  rw.read = _strRead;
  return rw;
}

RWStream rwStreamMkStrS(char *str, size_t size) {
  RWStream rw = {0};
  rw.size = size;
  rw.offs = 0;
  rw.data = (void *)str;
  rw.write = _strWrite;
  rw.peek = _strPeek;
  rw.tell = _strTell;
  rw.read = _strRead;
  return rw;
}

RWStream rwStreamMkFile(Stream *stream) {
  RWStream rw = {0};
  rw.data = stream;
  rw.write = _vfsWrite;
  rw.peek = _vfsPeek;
  rw.tell = _vfsTell;
  rw.read = _vfsRead;
  return rw;
}

////////////////////////////////////////////////////////////////////////////////

void _skipSpaces(RWStream *s) {
  char ch = 0;
  while (s->peek(s, &ch, 1) && ch == ' ') {
    s->read(s, &ch, 1);
  }
}

void _skipWhitespaces(RWStream *s) {
  char ch = 0;
  while (s->peek(s, &ch, 1) &&
         (ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r')) {
    s->read(s, &ch, 1);
  }
}

const char basetbl[256] = {
    ['0'] = 0,  ['1'] = 1,  ['2'] = 2,  ['3'] = 3,  ['4'] = 4,  ['5'] = 5,
    ['6'] = 6,  ['7'] = 7,  ['8'] = 8,  ['9'] = 9,  ['A'] = 10, ['B'] = 11,
    ['C'] = 12, ['D'] = 13, ['E'] = 14, ['F'] = 15, ['a'] = 10, ['b'] = 11,
    ['c'] = 12, ['d'] = 13, ['e'] = 14, ['f'] = 15};

unsigned long long fmtParseNum(RWStream *s, int base) {
  char buf[64] = {0};
  s->peek(s, buf, 64);
  char *nptr = buf;

  if (base == 0) {
    if (*nptr == '0') {
      if (nptr[1] == 'x' || nptr[1] == 'X') {
        base = 16;
        nptr += 2;
      } else {
        base = 8;
        nptr += 1;
      }
    } else {
      base = 10;
    }
  }

  unsigned long long n = 0;
  while (*nptr) {
    if (*nptr == '0' || (basetbl[*nptr] && basetbl[*nptr] < base)) {
      n = n * base + basetbl[*nptr];
    } else {
      break;
    }
    nptr++;
  }

  s->read(s, buf, nptr - buf);
  return n;
}

unsigned long long fmtParseULL(RWStream *s, int base) {
  _skipSpaces(s);
  char ch;

  s->peek(s, &ch, 1);
  if (ch == '+')
    s->read(s, &ch, 1);

  return fmtParseNum(s, base);
}

long long fmtParseLL(RWStream *s, int base) {
  _skipSpaces(s);
  char ch = 0;

  s->peek(s, &ch, 1);
  if (ch == '-') {
    s->read(s, &ch, 1);
    return -fmtParseNum(s, base);
  }
  if (ch == '+')
    s->read(s, &ch, 1);
  return fmtParseNum(s, base);
}

double fmtParseD(RWStream *s) {
  char ch = 0;
  _skipSpaces(s);

  double sign = 1;
  s->peek(s, &ch, 1);
  if (ch == '-') {
    s->read(s, &ch, 1);
    sign = -1;
  }
  if (ch == '+')
    s->read(s, &ch, 1);

  size_t point = s->tell(s);

  double val = 0;
  unsigned long long whole = fmtParseNum(s, 10);

  if (s->tell(s) == point) {
    return 0;
  }

  val = whole;
  s->peek(s, &ch, 1);
  if (ch == '.') {
    s->read(s, &ch, 1);
    unsigned long long frac = 0;
    double div = 1;
    while (s->peek(s, &ch, 1) && (ch >= '0' && ch <= '9')) {
      div *= 10;
      frac = frac * 10 + (ch - '0');
      s->read(s, &ch, 1);
    }
    val += frac / div;
  }

  s->peek(s, &ch, 1);
  if (ch == 'e' || ch == 'E') {
    s->read(s, &ch, 1);

    double expsign = 1;
    s->peek(s, &ch, 1);
    if (ch == '-') {
      s->read(s, &ch, 1);
      expsign = -1;
    }
    if (ch == '+')
      s->read(s, &ch, 1);

    double e = fmtParseNum(s, 10) * expsign;
    return val * exp(log(10) * e);
  }

  return val * sign;
}

////////////////////////////////////////////////////////////////////////////////

// TODO: Scanset
// TODO: Width
//
size_t fmtvscanf(RWStream *s, const char *fmt, va_list ap) {
  size_t num = 0;
  size_t start = s->tell(s);

  while (*fmt) {
    // char data[1024];
    // data[s->peek(s, data, 1024)] = 0;
    // printf("%s\n", data);

    _skipWhitespaces(s);
    if (*fmt == '%') {
      fmt++;
      size_t point = s->tell(s);

      switch (*fmt) {
      case '%': {
        char ch = 0;
        s->read(s, &ch, 1);
        num--;
        break;
      }
      case 'i': {
        int *i = va_arg(ap, int *);
        *i = fmtParseLL(s, 0);
        break;
      }
      case 'd': {
        int *i = va_arg(ap, int *);
        *i = fmtParseLL(s, 10);
        break;
      }
      case 'u': {
        unsigned int *i = va_arg(ap, unsigned int *);
        *i = fmtParseULL(s, 10);
        break;
      }
      case 'o': {
        unsigned int *i = va_arg(ap, unsigned int *);
        *i = fmtParseULL(s, 8);
        break;
      }
      case 'x': {
        unsigned int *i = va_arg(ap, unsigned int *);
        *i = fmtParseULL(s, 16);
        break;
      }
      case 'e':
      case 'g':
      case 'f': {
        double *d = va_arg(ap, double *);
        *d = fmtParseD(s);
        break;
      }
      case 'c': {
        char *c = va_arg(ap, char *);
        s->read(s, c, 1);
        break;
      }
      case 'p': {
        void **p = va_arg(ap, void **);
        *p = (void *)(size_t)fmtParseULL(s, 16);
        break;
      }
      case 's': {
        char *buf = va_arg(ap, char *);
        size_t i = 0;
        char ch = 0;
        while (s->peek(s, &ch, 1) && ch != ' ' && ch != '\n' && ch != '\t' &&
               ch != '\r') {
          s->read(s, &buf[i++], 1);
        }
        buf[i] = 0;
        break;
      }
      case 'n': {
        int *i = va_arg(ap, int *);
        *i = s->tell(s) - start;
        break;
      }
      }
      if (point == s->tell(s) && *fmt != 'n') {
        return num;
      } else {
        num++;
      }
    } else if (*fmt == ' ' || *fmt == '\n' || *fmt == '\t' || *fmt == '\r') {
      // Skip whitespace
    } else {
      char ch = 0;
      s->peek(s, &ch, 1);
      if (ch != *fmt) {
        return num;
      }
      s->read(s, &ch, 1);
    }

    fmt++;
  }

  return num;
}

size_t fmtscanf(RWStream *s, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  size_t num = fmtvscanf(s, fmt, ap);
  va_end(ap);
  return num;
}

////////////////////////////////////////////////////////////////////////////////

void fmtStringifyULL(RWStream *s, unsigned long long n, int base, bool upper) {
  char buf[64] = {0};
  char *ptr = buf + 63;
  *--ptr = 0;

  if (n == 0) {
    *--ptr = '0';
  }

  while (n) {
    *--ptr = (upper ? "0123456789ABCDEF" : "0123456789abcdef")[n % base];
    n /= base;
  }

  s->write(s, ptr, buf + 63 - ptr - 1);
}

void fmtStringifyLL(RWStream *s, long long n, int base, bool upper, bool sign,
                    bool padsign) {
  if (n < 0)
    sign = true;

  if (sign) {
    s->write(s, n < 0 ? "-" : "+", 1);
  } else if (padsign) {
    s->write(s, n < 0 ? "-" : " ", 1);
  }

  fmtStringifyULL(s, n < 0 ? -n : n, base, upper);
}

void fmtStringifyD(RWStream *s, double n, bool upper, bool sign,
                   bool scientific, int prec, bool padsign) {
  char buf[64] = {0};
  char *ptr = buf + 64;

  if (n < 0)
    sign = true;

  if (sign) {
    s->write(s, n < 0 ? "-" : "+", 1);
  } else if (padsign) {
    s->write(s, n < 0 ? "-" : " ", 1);
  }

  // TODO: Scientific
  if (n < 0) {
    n = -n;
  }

  unsigned long long whole = n;
  double frac = n - whole;

  fmtStringifyULL(s, whole, 10, upper);

  if (prec == 0)
    return;

  s->write(s, ".", 1);

  for (int i = 0; i < prec; i++) {
    frac *= 10;
    s->write(s, &"0123456789"[((int)frac % 10)], 1);
  }
}

////////////////////////////////////////////////////////////////////////////////

size_t fmtvprintf(RWStream *s, const char *fmt, va_list ap) {
  size_t start = s->supposedoffs;
  while (*fmt) {
    if (*fmt == '%') {
      fmt++;
      enum { XNONE, XHH, XH, XL, XLL, XJ, XZ, XT };
      enum { NNONE, NULONGLONG, NLONGLONG, NDOUBLE };

      bool leftjustify = false;
      bool forcesign = false;
      bool padsign = false;
      bool decorated = false;
      bool padzero = false;

      while (true) {
        switch (*fmt) {
        case '-':
          leftjustify = true;
          break;
        case '+':
          forcesign = true;
          break;
        case ' ':
          padsign = true;
          break;
        case '#':
          decorated = true;
          break;
        case '0':
          padzero = true;
          break;
        default:
          goto stop;
        }
        fmt++;
      }
    stop:;

      int width = 0;
      if (*fmt >= '0' && *fmt <= '9') {
        while (*fmt >= '0' && *fmt <= '9') {
          width = width * 10 + (*fmt - '0');
          fmt++;
        }
      } else if (*fmt == '*') {
        width = va_arg(ap, int);
        fmt++;
      }

      int precision = -1;
      if (*fmt == '.') {
        fmt++;
        if (*fmt >= '0' && *fmt <= '9') {
          precision = 0;
          while (*fmt >= '0' && *fmt <= '9') {
            precision = precision * 10 + (*fmt - '0');
            fmt++;
          }
        } else if (*fmt == '*') {
          precision = va_arg(ap, int);
          fmt++;
        }
      }

      int length = XNONE;
      if (*fmt == 'h') {
        fmt++;
        if (*fmt == 'h') {
          length = XHH;
          fmt++;
        } else {
          length = XH;
        }
      } else if (*fmt == 'l') {
        fmt++;
        if (*fmt == 'l') {
          length = XLL;
          fmt++;
        } else {
          length = XL;
        }
      } else if (*fmt == 'j') {
        length = XJ;
        fmt++;
      } else if (*fmt == 'z') {
        length = XZ;
        fmt++;
      } else if (*fmt == 't') {
        length = XT;
        fmt++;
      }

      bool uppercase = false;
      int numtype = NNONE;
      int base;
      unsigned long long ull;
      long long ll;
      double d;

      switch (*fmt) {
      case 'i':
      case 'd':
        base = 10;
        numtype = NLONGLONG;
        switch (length) {
        case XHH:
          ll = (char)va_arg(ap, int);
          break;
        case XH:
          ll = (short)va_arg(ap, int);
          break;
        case XL:
          ll = va_arg(ap, long);
          break;
        case XLL:
          ll = va_arg(ap, long long);
          break;
        case XJ:
          ll = va_arg(ap, intmax_t);
          break;
        case XZ:
          ll = va_arg(ap, size_t);
          break;
        case XT:
          ll = va_arg(ap, ptrdiff_t);
          break;
        default:
          ll = va_arg(ap, int);
          break;
        }
        break;
      case 'X':
        base = 16;
        uppercase = true;
        goto decide;
      case 'x':
        base = 16;
        goto decide;
      case 'o':
        base = 8;
        goto decide;
      case 'u':
        base = 10;
      decide:
        numtype = NULONGLONG;
        switch (length) {
        case XHH:
          ull = va_arg(ap, int);
          break;
        case XH:
          ull = va_arg(ap, int);
          break;
        case XL:
          ull = va_arg(ap, unsigned long);
          break;
        case XLL:
          ull = va_arg(ap, unsigned long long);
          break;
        case XJ:
          ull = va_arg(ap, uintmax_t);
          break;
        case XZ:
          ull = va_arg(ap, size_t);
          break;
        case XT:
          ull = va_arg(ap, ptrdiff_t);
          break;
        default:
          ull = va_arg(ap, int);
          break;
        }
        break;
      case 'E':
      case 'e':
      case 'G':
      case 'g':
      case 'f': {
        d = va_arg(ap, double);
        if (precision < 0)
          precision = 1;
        numtype = NDOUBLE;
        break;
      }
      case 'P':
        uppercase = true;
      case 'p': {
        numtype = NULONGLONG;
        base = 16;
        decorated = true;
        ull = (size_t)va_arg(ap, void *);
        break;
      }
      case '%': {
        if (!leftjustify)
          for (int i = 0; i < width - 1; i++)
            s->write(s, " ", 1);

        s->write(s, "%", 1);

        if (leftjustify)
          for (int i = 0; i < width - 1; i++)
            s->write(s, " ", 1);

        goto end;
      }
      case 'c': {
        char c = va_arg(ap, int);

        if (!leftjustify)
          for (int i = 0; i < width - 1; i++)
            s->write(s, " ", 1);

        s->write(s, &c, 1);

        if (leftjustify)
          for (int i = 0; i < width - 1; i++)
            s->write(s, " ", 1);

        goto end;
      }
      case 's': {
        // NOTE: No wchar.
        char *str = va_arg(ap, char *);
        int strl = strlen(str);
        int size = strl;
        if (precision != -1 && precision < strl)
          size = precision;

        if (!leftjustify)
          for (int i = 0; i < width - size; i++)
            s->write(s, " ", 1);

        s->write(s, str, size);

        if (leftjustify)
          for (int i = 0; i < width - size; i++)
            s->write(s, " ", 1);
        goto end;
      }
      case 'n': {
        if (!leftjustify)
          for (int i = 0; i < width; i++)
            s->write(s, " ", 1);

        switch (length) {
        case XHH:
          *va_arg(ap, signed char *) = s->tell(s) - start;
        case XH:
          *va_arg(ap, short *) = s->tell(s) - start;
        case XL:
          *va_arg(ap, long *) = s->tell(s) - start;
        case XLL:
          *va_arg(ap, long long *) = s->tell(s) - start;
        case XJ:
          *va_arg(ap, intmax_t *) = s->tell(s) - start;
        case XZ:
          *va_arg(ap, size_t *) = s->tell(s) - start;
        case XT:
          *va_arg(ap, ptrdiff_t *) = s->tell(s) - start;
        default:
          *va_arg(ap, int *) = s->tell(s) - start;
        }

        if (leftjustify)
          for (int i = 0; i < width; i++)
            s->write(s, " ", 1);

        goto end;
      }
      }

      char numbuf[128] = {0};
      RWStream numstream = rwStreamMkStrS(numbuf, 128);

      switch (numtype) {
      case NULONGLONG:
        fmtStringifyULL(&numstream, ull, base, uppercase);
        break;
      case NLONGLONG:
        fmtStringifyLL(&numstream, ll, base, uppercase, forcesign, padsign);
        break;
      case NDOUBLE:
        fmtStringifyD(&numstream, d, uppercase, forcesign, false, precision,
                      padsign);
        break;
      default:
        break;
      }

      int numlen = numstream.tell(&numstream);
      int padding = width - numlen;

      if (numlen < width) {
        if (leftjustify) {
          if (decorated) {
            if (base == 16) {
              s->write(s, "0x", 2);
              padding -= 2;
            } else if (base == 8) {
              s->write(s, "0", 1);
              padding -= 1;
            }
          }
          if (precision > 0 && numtype != NDOUBLE) {
            for (int i = 0; i < precision - numlen; i++) {
              s->write(s, "0", 1);
              padding -= 1;
            }
          }
          s->write(s, numbuf, numlen);
          for (int i = 0; i < padding; i++) {
            s->write(s, " ", 1);
          }
        } else {
          if (decorated) {
            if (base == 16) {
              if (padzero)
                s->write(s, "0x", 2);
              padding -= 2;
            } else if (base == 8) {
              if (padzero)
                s->write(s, "0", 1);
              padding -= 1;
            }
          }
          if (precision > 0 && numtype != NDOUBLE) {
            padding -= precision - numlen;
          }
          for (int i = 0; i < padding; i++) {
            s->write(s, padzero ? "0" : " ", 1);
          }
          if (decorated && !padzero) {
            if (base == 16) {
              s->write(s, "0x", 2);
            } else if (base == 8) {
              s->write(s, "0", 1);
            }
          }
          if (precision > 0 && numtype != NDOUBLE) {
            for (int i = 0; i < precision - numlen; i++)
              s->write(s, "0", 1);
          }
          s->write(s, numbuf, numlen);
        }
      } else {
        if (decorated) {
          if (base == 16) {
            s->write(s, "0x", 2);
          } else if (base == 8) {
            s->write(s, "0", 1);
          }
        }
        if (precision > 0 && numtype != NDOUBLE) {
          for (int i = 0; i < precision - numlen; i++)
            s->write(s, "0", 1);
        }
        s->write(s, numbuf, numlen);
      }
    end:;
      fmt++;
    } else {
      s->write(s, fmt, 1);
      fmt++;
    }
  }

  return s->supposedoffs - start;
}

size_t fmtprintf(RWStream *s, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  size_t num = fmtvprintf(s, fmt, ap);
  va_end(ap);
  return num;
}
