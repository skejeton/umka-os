#include "vfs.h"
#include <assert.h>
#include <stdio.h>

int main() {
  vfsInit();
  Stream *s = vfsOpen("test.txt", "wb");
  assert(s);
  vfsWrite(s, "hello", 5);
  vfsClose(s);

  // now read
  s = vfsOpen("./test.txt", "rb");
  char buf[10];
  vfsSeek(s, 0, 2);
  size_t size = vfsTell(s);
  vfsSeek(s, 0, 0);
  vfsRead(s, buf, size);
  assert(vfsEof(s));
  buf[size] = '\0';
  printf("%s\n", buf);
  assert(strcmp(buf, "hello") == 0);
  vfsClose(s);

  assert(vfsRemove("./test.txt") == 0);
  assert(vfsRemove("./test.txt") == -1);
  assert(!vfsOpen("./test.txt", "r"));
  assert(vfsOpen("./test.txt", "w"));
  assert(vfsOpen("./test.txt", "r"));
  assert(vfsRemove("./././test.txt") == 0);

  s = vfsOpen("test.txt", "rb");
  assert(s == NULL);

  assert(vfsCreate("test1.txt") == 0);
  assert(vfsCreate("test2.txt") == 0);
  assert(vfsCreate("subdir") == 0);
  assert(vfsCreate("subdir/test1.txt") == 0);
  assert(vfsExists("test1.txt"));
  assert(vfsExists("test2.txt"));
  assert(vfsExists("subdir"));
  assert(vfsExists("subdir/test1.txt"));
  assert(!vfsExists("subdir/test2.txt"));
  assert(vfsExists("subdir/../subdir/test1.txt"));

  printf("OK\n");
}