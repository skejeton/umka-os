#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct Stream Stream;
typedef struct FSO FSO;

void vfsInit();
Stream *vfsOpen(const char *path, const char *perm);
void vfsClose(Stream *);
size_t vfsRead(Stream *, void *, size_t);
size_t vfsWrite(Stream *, const void *, size_t);
void vfsSeek(Stream *, size_t pos, size_t whence);
size_t vfsTell(Stream *);
int vfsEof(Stream *);
void vfsHookFlush(Stream *, void *, void (*)(void *, void *, size_t));
void vfsFlush(Stream *);
bool vfsExists(const char *);
int vfsCreate(const char *);
int vfsRemove(const char *);

char *vfsFSOName(FSO *fso);
Stream *vfsFSOOpen(FSO *fso, const char *perm);
FSO *vfsRoot();
FSO *vfsNext(FSO *of);
FSO *vfsChild(FSO *of);
