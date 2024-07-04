#include "umka.h"
#include "vfs.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern char _binary_bin_fs_bin_start[];
extern char _binary_bin_fs_bin_end[];

char *loadVfsImage(char *p, char *path) {
  char temp[512];
  int l;
  Stream *s;

  while (p < _binary_bin_fs_bin_end) {
    switch (*p) {
    case 'p':
      return p + 1;
    case 'd':
      p++;
      l = strlen(p);
      sprintf(temp, "%s/%s", path, p);
      vfsCreate(temp);
      p = loadVfsImage(p + l + 1, temp);
      break;
    case 'f':
      p++;
      l = strlen(p);
      sprintf(temp, "%s/%s", path, p);
      vfsCreate(temp);
      p += l + 1;
      uint32_t size = *(uint32_t *)(p);
      p += 4;
      s = vfsOpen(temp, "wb");
      vfsWrite(s, p, size);
      vfsClose(s);
      p += size;
      break;
    default:
      asm("cli\nhlt");
    }
  }

  return p;
}

extern void irq1();
extern void irq12();
void handleIrq1() { umkaRuntimeHandleIRQ(1); }
void handleIrq12() { umkaRuntimeHandleIRQ(12); }

void __stack_chk_fail_local() {}

extern void kmain() {
  rtlInit((void *)0x1000000);
  loadVfsImage(_binary_bin_fs_bin_start, "");
  umkaRuntimeCompile("main.um");
  umkaRuntimeRegister("irq1", irq1);
  umkaRuntimeRegister("irq12", irq12);
  umkaRuntimeInit();
  umkaRuntimeSchedule();
  asm("sti");

  while (1) {
    asm("hlt");
    umkaRuntimeSchedule();
  }
}