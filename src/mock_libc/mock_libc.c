#include "mock_libc.h"
#include "alloc.h"
#include "fmt.h"
#include "vfs.h"
#include <stdarg.h>
#include <stdint.h>

#ifndef _WIN32
int _errno;
#else
int *_errno() {
  static int e;
  return &e;
}
#endif

// ALLOC
#ifdef USE_VIRTUALALLOC
#include <windows.h>
void free(void *ptr) { VirtualFree(ptr, 0, MEM_RELEASE); }

void *realloc(void *ptr, size_t size) {
  void *new = malloc(size);
  memcpy(new, ptr, size);
  free(ptr);
  return new;
}

void *malloc(size_t size) {
  return VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
}
void *calloc(size_t nmemb, size_t size) {
  void *ptr = malloc(nmemb * size);
  for (size_t i = 0; i < nmemb * size; i++) {
    ((char *)ptr)[i] = 0;
  }
  return ptr;
}
#else
void free(void *ptr) { allocatorFree(ptr); }
void *realloc(void *ptr, size_t size) { return allocatorRealloc(ptr, size); }
void *malloc(size_t size) { return allocatorAlloc(size); }
void *calloc(size_t nmemb, size_t size) {
  void *ptr = allocatorAlloc(nmemb * size);
  for (size_t i = 0; i < nmemb * size; i++) {
    ((char *)ptr)[i] = 0;
  }
  return ptr;
}
#endif

// MEMORY
int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = s1;
  const unsigned char *p2 = s2;
  for (int i = 0; i < n; i++) {
    if (p1[i] != p2[i]) {
      return p1[i] - p2[i];
    }
  }
  return 0;
}

void *memcpy(void *dest, const void *src, size_t n) {
  char *d = dest;
  const char *s = src;
  while (n--) {
    *d++ = *s++;
  }
  return dest;
}

void *memset(void *s, int c, size_t n) {
  unsigned char *p = s;
  while (n--) {
    *p++ = c;
  }
  return s;
}

void *memmove(void *dest, const void *src, size_t n) {
  char *d = dest;
  const char *s = src;
  if (d < s) {
    while (n--) {
      *d++ = *s++;
    }
  } else {
    d += n;
    s += n;
    while (n--) {
      *--d = *--s;
    }
  }
  return dest;
}

// FILE IO
typedef void FILE;
FILE *stdin;
FILE *stdout;
FILE *stderr;

int open(const char *pathname, int flags) {
  if (flags & O_RDONLY) {
    return (int)(size_t)vfsOpen(pathname, "rb");
  } else {
    return (int)(size_t)vfsOpen(pathname, "wb");
  }
}

int close(int fd) {
  vfsClose((FILE *)(size_t)fd);
  return 0;
}
int read(int fd, void *buf, size_t count) {
  return vfsRead((FILE *)(size_t)fd, buf, count);
}
int lseek(int fd, int offset, int whence) {
  return vfsSeek((FILE *)(size_t)fd, offset, whence), 0;
}

FILE *fopen(const char *path, const char *mode) { return vfsOpen(path, mode); }

int fclose(FILE *stream) {
  vfsClose(stream);
  return 0;
}

int fgetc(FILE *stream) {
  char c;
  if (vfsRead(stream, &c, 1) == 0) {
    return EOF;
  }
  return c;
}

int fputc(char c, FILE *stream) {
  if (vfsWrite(stream, &c, 1) == 0) {
    return EOF;
  }
  return c;
}

int fseek(FILE *stream, long int offset, int whence) {
  vfsSeek(stream, offset, whence);
  return 0;
}

long int ftell(FILE *stream) { return vfsTell(stream); }

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  return vfsRead(stream, ptr, size * nmemb) / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
  return vfsWrite(stream, ptr, size * nmemb) / size;
}

int feof(FILE *stream) { return vfsEof(stream); }

int fflush(FILE *stream) {
  vfsFlush(stream);
  return 0;
}

int ferror(FILE *stream) { return 0; }

int rewind(FILE *stream) {
  vfsSeek(stream, 0, 0);
  return 0;
}

int remove(const char *filename) { return vfsRemove(filename); }

int puts(const char *s) {
  fwrite(s, 1, strlen(s), stdout);
  fputc('\n', stdout);
  return 0;
}

// FORMATTING
char *strncpy(char *d, const char *s, size_t n) {
  size_t i = 0;
  while (i < n && s[i]) {
    d[i] = s[i];
    i++;
  }
  d[i] = 0;
  return d;
}

char *strcpy(char *d, const char *s) {
  size_t i = 0;
  while (s[i]) {
    d[i] = s[i];
    i++;
  }
  d[i] = 0;
  return d;
}

int strcmp(const char *a, const char *b) {
  while (*a && *b && *a == *b) {
    a++;
    b++;
  }
  return *a - *b;
}

int strncmp(const char *a, const char *b, size_t n) {
  while (n-- && *a && *b && *a == *b) {
    a++;
    b++;
  }
  return n ? *a - *b : 0;
}

size_t strlen(const char *s) {
  size_t i = 0;
  while (s[i]) {
    i++;
  }
  return i;
}

char *strcat(char *d, const char *s) {
  size_t i = 0;
  while (d[i]) {
    i++;
  }
  size_t j = 0;
  while (s[j]) {
    d[i++] = s[j++];
  }
  d[i] = 0;
  return d;
}

char *strncat(char *d, const char *s, size_t c) {
  size_t i = 0;
  while (d[i]) {
    i++;
  }
  size_t j = 0;
  while (s[j] && j < c) {
    d[i++] = s[j++];
  }
  d[i] = 0;
  return d;
}

char *strrchr(const char *s, int ch) {
  char *p = NULL;
  while (*s) {
    if (*s == ch) {
      p = (char *)s;
    }
    s++;
  }
  return p;
}

unsigned long long strtoull(const char *nptr, char **endptr, int base) {
  RWStream stream = rwStreamMkStr((char *)nptr);

  unsigned long long num = fmtParseULL(&stream, base);

  if (endptr) {
    *endptr = (char *)nptr + stream.offs;
  }

  return num;
}

long long strtoll(const char *nptr, char **endptr, int base) {
  RWStream stream = rwStreamMkStr((char *)nptr);

  long long num = fmtParseLL(&stream, base);

  if (endptr) {
    *endptr = (char *)nptr + stream.offs;
  }

  return num;
}

long strtol(const char *nptr, char **endptr, int base) {
  RWStream stream = rwStreamMkStr((char *)nptr);

  long num = fmtParseLL(&stream, base);

  if (endptr) {
    *endptr = (char *)nptr + stream.offs;
  }

  return num;
}

double strtod(const char *nptr, char **endptr) {
  RWStream stream = rwStreamMkStr((char *)nptr);

  double num = fmtParseD(&stream);

  if (endptr) {
    *endptr = (char *)nptr + stream.offs;
  }

  return num;
}

int vsscanf(const char *s, const char *fmt, va_list ap) {
  RWStream stream = rwStreamMkStr((char *)s);
  size_t n = fmtvscanf(&stream, fmt, ap);
  return n;
}

int vfscanf(FILE *f, const char *fmt, va_list ap) {
  RWStream stream = rwStreamMkFile(f);
  size_t n = fmtvscanf(&stream, fmt, ap);
  return n;
}

int sscanf(const char *s, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsscanf(s, fmt, ap);
  va_end(ap);
  return n;
}

int vsnprintf(char *dest, size_t n, const char *fmt, va_list ap) {
  RWStream stream = rwStreamMkStrS(dest, n);
  size_t written = fmtvprintf(&stream, fmt, ap);

  return written;
}

int vfprintf(FILE *f, const char *fmt, va_list ap) {
  RWStream stream = rwStreamMkFile(f);
  size_t written = fmtvprintf(&stream, fmt, ap);

  return written;
}

int sprintf(char *dest, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(dest, 0xFFFFFFFF, fmt, ap);
  va_end(ap);
  return n;
}

int snprintf(char *dest, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  n = vsnprintf(dest, n, fmt, ap);
  va_end(ap);
  return n;
}

int fprintf(FILE *f, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int n = vfprintf(f, fmt, ap);
  va_end(ap);
  return n;
}

// MATH

double trunc(double x) {
  if (x < 0) {
    return ceil(x);
  }
  return floor(x);
}

double fmod(double x, double y) { return x - y * floor(x / y); }
double fabs(double x) { return x < 0 ? -x : x; }

double sin(double x) {
  x *= 180 / M_PI;
  x = fmod(x, M_2PI);
  if (x / 2 == 1) {
    x -= M_PI;
    return -(4 * x * (180 - x) / (40500 - x * (180 - x)));
  }

  return 4 * x * (180 - x) / (40500 - x * (180 - x));
}

double cos(double x) { return sin(x + M_PI_2); }
double atan(double x) {
  return M_PI_4 * x - x * (fabs(x) - 1) * (0.2447 + 0.0663 * fabs(x));
}
double atan2(double x, double y) {
  if (x == 0) {
    return y > 0 ? M_PI_2 : -M_PI_2;
  }
  if (y == 0) {
    return x > 0 ? 0 : M_PI;
  }
  double z = x / y;
  if (z < 0) {
    return atan(z) - M_PI;
  }
  return atan(z);
}
double sqrt(double x) {
  double z = x;
  for (int i = 0; i < 10; i++) {
    z = (z + x / z) / 2;
  }
  return z;
}
double floor(double x) {
  int n = (int)x;
  return n > x ? n - 1 : n;
}
double ceil(double x) {
  int n = (int)x;
  return n < x ? n + 1 : n;
}
double round(double x) {
  int n = (int)x;
  return x - n >= 0.5 ? n + 1 : n;
}

// taken from here
// https://gist.github.com/jrade/293a73f89dfef51da6522428c857802d
double exp(double x) {
  double a = (1ll << 52) / 0.6931471805599453;
  double b = (1ll << 52) * (1023 - 0.04367744890362246);
  x = a * x + b;

  double c = (1ll << 52);
  double d = (1ll << 52) * 2047;
  if (x < c || x > d)
    x = (x < c) ? 0.0 : d;

  uint64_t n = (uint64_t)x;
  memcpy(&x, &n, 8);
  return x;
}

double log(double _x) {
  float x = _x;

  unsigned int bx = *(unsigned int *)(&x);
  unsigned int ex = bx >> 23;
  signed int t = (signed int)ex - (signed int)127;
  unsigned int s = (t < 0) ? (-t) : t;
  bx = 1065353216 | (bx & 8388607);
  x = *(float *)(&bx);
  return -1.49278 + (2.11263 + (-0.729104 + 0.10969 * x) * x) * x +
         0.6931471806 * t;
}

// STUBS
int system(const char *cmd) { return 0; }
void *dlopen(const char *dll, int flags) { return NULL; }
int dlclose(void *dll) { return 0; }
void *dlsym(void *dll, const char *sym) { return NULL; }
char *getcwd(char *to, size_t n) {
  if (n >= 2) {
    to[0] = '/';
    to[1] = '\0';
  } else if (n == 1) {
    to[0] = '\0';
  }
  return to;
};
int time(int *d) { return *d = 0, 0; }
int clock_gettime(int clock, struct timespec *t) {
  *t = (struct timespec){0};
  return 0;
}

struct tm *localtime(const time_t *timer) {
  static struct tm tm;
  return &tm;
}

struct tm *gmtime(const time_t *timer) {
  static struct tm tm;
  return &tm;
}

time_t mktime(struct tm *tm) { return 0; }

char *getenv(const char *k) { return NULL; }
clock_t clock() {
  static clock_t c;
  return c;
}

// setjmp/longjmp
#ifdef _WIN32
// imported from msvcrt
#else
// implemented in lib.s
#endif

void rtlInit(void *mem) {
  allocatorInit(mem);
  vfsInit();
  stdin = fopen("$stdin", "rb");
  stdout = fopen("$stdout", "wb");
  stderr = fopen("$stderr", "wb");
}
