#include "alloc.h"
#include <assert.h>
#include <string.h>
#include <windows.h>

int main() {
  allocatorInit(VirtualAlloc(NULL, 1 << 16, MEM_COMMIT, PAGE_READWRITE));

  char *data = allocatorAlloc(50);
  char *origdata = data;
  memcpy(data, "Hello world", 12);
  data[12] = 0;
  char *oldata = data;
  assert(strcmp(data, "Hello world") == 0);
  data = allocatorRealloc(data, 1000);
  assert(oldata != data);
  assert(strcmp(data, "Hello world") == 0);
  char *data2 = allocatorAlloc(50);
  assert(data2 == origdata);
  memcpy(data2, "Cool", 5);
  data2[5] = 0;
  assert(strcmp(data2, "Cool") == 0);
  assert(strcmp(data, "Hello world") == 0);
  allocatorFree(data);
  allocatorFree(data2);
  char *data3 = allocatorAlloc(50);
  char *data4 = allocatorAlloc(50);
  char *data5 = allocatorAlloc(50);
  char *data6 = allocatorAlloc(50);
  char *data7 = allocatorAlloc(50);
  char *data8 = allocatorAlloc(50);
  data8 = allocatorAlloc(10000);
  assert(data3 == origdata);
  assert(data4 == data);
  assert(data5 != data && data5 != origdata);
  allocatorFree(data7);
  allocatorFree(data8);
  assert(allocatorAlloc(50) == data8);
  assert(allocatorAlloc(50) == data7);
  printf("OK\n");
}