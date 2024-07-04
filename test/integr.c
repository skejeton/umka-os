#include "../src/umka/src/umka_api.h"
#include "mock_libc.h"
#include "vfs.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// why microsoft
int _fltused;

void outputdbg(void *data, void *buf, size_t size) {
  WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), buf, size, NULL, NULL);
}

const char source[] = "import \"std.um\"\n"
                      "fn indirect() { printf(\"Hello umka!\\n\"); }\n"
                      "fn test(f: fn())\n"
                      "fn init*() { test(indirect); }\n";

void testextern(UmkaStackSlot *p, UmkaStackSlot *r) {
  fprintf(stderr, "OK enter\n");
  umkaCall(umkaGetAPI(r->ptrVal), p[0].intVal, 0, NULL, NULL);
  fprintf(stderr, "OK leave\n");
}

int __stdcall mainCRTStartup() {
  rtlInit(VirtualAlloc(0, 1 << 26, MEM_COMMIT, PAGE_READWRITE));
  vfsHookFlush(stdout, NULL, outputdbg);
  vfsHookFlush(stderr, NULL, outputdbg);
  FILE *s = fopen("main.um", "wb");
  fwrite(source, strlen(source) + 1, 1, s);
  fclose(s);

  void *umka = umkaAlloc();

  bool umkaOk = umkaInit(umka, "main.um", NULL, 1024 * 1024, NULL, 0, NULL,
                         true, false, NULL);

  umkaAddFunc(umka, "test", testextern);

  if (!umkaOk) {
    fprintf(stderr, "umkaInit() failed: %s\n", umkaGetError(umka)->msg);
    return 1;
  }

  if (!umkaCompile(umka)) {
    fprintf(stderr, "umkaCompile() failed: %s\n", umkaGetError(umka)->msg);
    return 1;
  }

  fprintf(stdout, "%s", umkaAsm(umka));

  umkaCall(umka, umkaGetFunc(umka, NULL, "init"), 0, NULL, NULL);
  fprintf(stderr, "OK exit\n");

  if (umkaRun(umka) != 0) {
    fprintf(stderr, "umkaRun() failed: %s\n", umkaGetError(umka)->msg);
    return 1;
  }

  return 0;
}