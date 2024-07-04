#pragma once

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define EOF 0
#define M_PI 3.14159265
#define M_2PI (3.14159265 * 2)
#define M_PI_2 (M_PI / 2)
#define M_PI_4 (M_PI / 4)
#define ERANGE 34
#define CLOCKS_PER_SEC 1000
#define CLOCK_REALTIME 0
#define O_RDONLY 1
#ifndef _WIN32
#define errno _errno
#define HUGE_VAL (__builtin_huge_val())
#else
static const union {
  unsigned char __c[8];
  double __d;
} __huge_val = {{0, 0, 0, 0, 0, 0, 0xf0, 0x7f}};
#define HUGE_VAL (__huge_val.__d)
#endif

#ifndef _WIN32
typedef struct __attribute__((aligned(4))) _aligned4 {
  int32_t x;
} _aligned4;

typedef _aligned4 jmp_buf[6];
#else
typedef struct __declspec(align(16)) _aligned16 {
  uint64_t x[2];
} _aligned16;

typedef _aligned16 jmp_buf[16];
#endif
#ifndef _WIN32
extern int _errno;
#endif

void free(void *ptr);
void *realloc(void *ptr, size_t size);
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);

int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);

typedef void FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int open(const char *pathname, int flags);
int close(int fd);
int read(int fd, void *buf, size_t count);
int lseek(int fd, int offset, int whence);

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
int fgetc(FILE *stream);
int fseek(FILE *stream, long int offset, int whence);
long int ftell(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int feof(FILE *stream);
int fflush(FILE *stream);
int ferror(FILE *stream);
int rewind(FILE *stream);
int remove(const char *filename);
int puts(const char *s);

static inline bool isalnum(int c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
         (c >= 'a' && c <= 'z');
}

static inline bool isalpha(int c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static inline bool isspace(int c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
         c == '\f';
}

long strtol(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);
double strtod(const char *nptr, char **endptr);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t n);
size_t strlen(const char *);
char *strcat(char *, const char *);
char *strncat(char *, const char *, size_t);
int sprintf(char *, const char *, ...);
int snprintf(char *, size_t, const char *, ...);
int vsnprintf(char *, size_t, const char *, va_list);
int sscanf(const char *, const char *, ...);
int vsscanf(const char *, const char *, va_list);
char *strncpy(char *, const char *, size_t);
char *strcpy(char *, const char *);
char *strrchr(const char *, int ch);
int fprintf(FILE *, const char *, ...);
int vfprintf(FILE *, const char *, va_list);
int vfscanf(FILE *, const char *, va_list);

double fabs(double x);
double trunc(double x);
double sin(double x);
double cos(double x);
double atan(double x);
double atan2(double x, double y);
double sqrt(double x);
double floor(double x);
double ceil(double x);
double round(double x);
double exp(double x);
double log(double x);
double fmod(double x, double y);

typedef long long time_t;

struct timespec {
  time_t tv_sec;
  long tv_nsec;
};

struct tm {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
};

typedef long clock_t;

int system(const char *);
void *dlopen(const char *, int);
int dlclose(void *);
void *dlsym(void *, const char *);
char *getcwd(char *, size_t);
int time(int *);
int clock_gettime(int clock, struct timespec *t);
struct tm *localtime(const time_t *timer);
struct tm *gmtime(const time_t *timer);
time_t mktime(struct tm *);
char *getenv(const char *);
clock_t clock();

#ifndef _WIN32
void longjmp(jmp_buf, int);
int setjmp(jmp_buf);
#endif

void rtlInit(void *mem);

#ifndef _WIN32
void panic(const char *s, ...);
#define assert(x)                                                              \
  if (!(x))                                                                    \
  panic("?! %s:%d (%s) ", __FILE__, __LINE__, (#x))
#else
#define assert(x)                                                              \
  if (!(x))                                                                    \
    ;
#endif

#if __USE_MINGW_ANSI_STDIO == 1
#endif