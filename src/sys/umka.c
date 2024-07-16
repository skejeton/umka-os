#include "umka.h"
#include "gfx.h"
#include "umka_api.h"
#include "vfs.h"
#include <stdio.h>
#include <time.h>

void panic(const char *fmt, ...);

static void *umka;
enum { TYPE_UINT8, TYPE_UINT16, TYPE_UINT32, TYPE_UINT8ARRAY };
static void *umkaTypes[32];
UmkaFuncContext umkaRegisterFn;
UmkaFuncContext umkaInitFn;
UmkaFuncContext umkaIRQFn;
UmkaFuncContext umkaScheduleFn;
static _Atomic int umkaLock = 0;

void umkaPrintError() {
  panic("Error: %s:%d %s\n", umkaGetError(umka)->fileName,
        umkaGetError(umka)->line, umkaGetError(umka)->msg);
}

void warning(UmkaError *error) {
  panic("Warning: %s:%d %s\n", error->fileName, error->line, error->msg);
}

////////////////////////////////////////////////////////////////////////////////

static void api__panic(UmkaStackSlot *p, UmkaStackSlot *r) {
  panic("(Umka) %s\n", (char *)umkaGetParam(p, 0)->ptrVal);
}

static void api__asm_in8(UmkaStackSlot *p, UmkaStackSlot *r) {
  uint8_t result;
  uint16_t port = umkaGetParam(p, 0)->uintVal;
  asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));

  umkaGetResult(p, r)->uintVal = result;
  r->uintVal = result;
}

static void api__asm_out8(UmkaStackSlot *p, UmkaStackSlot *r) {
  uint16_t port = umkaGetParam(p, 0)->uintVal;
  uint8_t val = umkaGetParam(p, 1)->uintVal;

  asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void api__asm_lidt(UmkaStackSlot *p, UmkaStackSlot *r) {
  uint32_t base = umkaGetParam(p, 0)->uintVal;
  uint16_t limit = umkaGetParam(p, 1)->uintVal;

  struct {
    uint16_t limit;
    uint32_t base;
  } __attribute__((packed)) idtr = {limit, base};

  asm volatile("lidt %0" : : "m"(idtr));
}

static void api__asm_cli(UmkaStackSlot *p, UmkaStackSlot *r) { asm("cli"); }
static void api__asm_sti(UmkaStackSlot *p, UmkaStackSlot *r) { asm("sti"); }

static void api__setupTypePtr(UmkaStackSlot *p, UmkaStackSlot *r) {
  umkaTypes[umkaGetParam(p, 0)->uintVal] = umkaGetParam(p, 1)->ptrVal;
}

static void api__getaddr(UmkaStackSlot *p, UmkaStackSlot *r) {
  umkaGetResult(p, r)->uintVal = umkaGetParam(p, 0)->uintVal;
}

static void api__getptr(UmkaStackSlot *p, UmkaStackSlot *r) {
  umkaGetResult(p, r)->ptrVal = umkaGetParam(p, 0)->ptrVal;
}

static void api__writes8(UmkaStackSlot *p, UmkaStackSlot *r) {
  void *dest = umkaGetParam(p, 0)->ptrVal;
  UmkaDynArray(uint8_t) *src = umkaGetParam(p, 1)->ptrVal;

  memcpy(dest, src->data, umkaGetDynArrayLen(src));
}

static void api__reads8(UmkaStackSlot *p, UmkaStackSlot *r) {
  void *data = umkaGetParam(p, 0)->ptrVal;
  size_t len = umkaGetParam(p, 1)->uintVal;

  UmkaDynArray(uint8_t) *dest =
      umkaAllocData(umka, sizeof(UmkaDynArray(uint8_t)), NULL);
  umkaMakeDynArray(umka, dest, umkaTypes[TYPE_UINT8ARRAY], len);

  memcpy(dest->data, data, len);

  umkaGetResult(p, r)->ptrVal = dest;
}

static void api__reads(UmkaStackSlot *p, UmkaStackSlot *r) {
  void *data = umkaGetParam(p, 0)->ptrVal;
  size_t len = umkaGetParam(p, 1)->uintVal;

  char *str = malloc(len + 1);
  memcpy(str, p[1].ptrVal, len);
  str[len] = '\0';

  umkaGetResult(p, r)->ptrVal = umkaMakeStr(umka, str);

  free(str);
}

static void api__gfxSetColor(UmkaStackSlot *p, UmkaStackSlot *r) {
  gfxSetColor(umkaGetParam(p, 0)->intVal, umkaGetParam(p, 1)->intVal,
              umkaGetParam(p, 2)->intVal, umkaGetParam(p, 3)->intVal);
}

static void api__gfxDrawBackground(UmkaStackSlot *p, UmkaStackSlot *r) {
  gfxDrawBackground();
}

static void api__gfxDrawRect(UmkaStackSlot *p, UmkaStackSlot *r) {
  gfxDrawRect(umkaGetParam(p, 0)->intVal, umkaGetParam(p, 1)->intVal,
              umkaGetParam(p, 2)->intVal, umkaGetParam(p, 3)->intVal);
}

static void api__gfxDrawClip(UmkaStackSlot *p, UmkaStackSlot *r) {
  gfxDrawClip(umkaGetParam(p, 0)->intVal, umkaGetParam(p, 1)->intVal,
              umkaGetParam(p, 2)->intVal, umkaGetParam(p, 3)->intVal);
}

static void api__gfxDrawIcon(UmkaStackSlot *p, UmkaStackSlot *r) {
  gfxDrawIcon(umkaGetParam(p, 0)->intVal, umkaGetParam(p, 1)->intVal,
              umkaGetParam(p, 2)->intVal);
}

static void api__gfxDrawChar(UmkaStackSlot *p, UmkaStackSlot *r) {
  gfxDrawChar(umkaGetParam(p, 0)->intVal, umkaGetParam(p, 1)->intVal,
              umkaGetParam(p, 2)->intVal);
}

static void api__gfxDims(UmkaStackSlot *p, UmkaStackSlot *r) {
  int w, h;
  gfxDims(&w, &h);
  *(int *)umkaGetParam(p, 0)->ptrVal = w;
  *(int *)umkaGetParam(p, 1)->ptrVal = h;
}

static void api__gfxSwap(UmkaStackSlot *p, UmkaStackSlot *r) { gfxSwap(); }

static void umka__flush(void *data, void *buf, size_t size) {
  UmkaFuncContext *ctx = data;

  umkaGetParam(ctx->params, 0)->ptrVal = buf;
  umkaGetParam(ctx->params, 1)->uintVal = size;

  umkaIncRef(umka, umkaGetUpvalue(ctx->params));

  umkaCall(umka, ctx);
  if (!umkaAlive(umka)) {
    umkaPrintError();
  }
}

static void api__vfsHookFlush(UmkaStackSlot *p, UmkaStackSlot *r) {
  Stream *s = umkaGetParam(p, 0)->ptrVal;
  UmkaClosure *closure = (UmkaClosure *)umkaGetParam(p, 1);
  void *callbackType = umkaGetParam(p, 2)->ptrVal;

  UmkaFuncContext *ctx = calloc(1, sizeof(UmkaFuncContext));
  umkaMakeFuncContext(umka, callbackType, closure->entryOffset, ctx);
  *umkaGetUpvalue(ctx->params) = closure->upvalue;

  vfsHookFlush(s, ctx, umka__flush);
}

static void api__vfsRoot(UmkaStackSlot *p, UmkaStackSlot *r) {
  umkaGetResult(p, r)->ptrVal = vfsRoot();
}

static void api__vfsNext(UmkaStackSlot *p, UmkaStackSlot *r) {
  umkaGetResult(p, r)->ptrVal = vfsNext((FSO *)umkaGetParam(p, 0)->ptrVal);
}

static void api__vfsChild(UmkaStackSlot *p, UmkaStackSlot *r) {
  umkaGetResult(p, r)->ptrVal = vfsChild((FSO *)umkaGetParam(p, 0)->ptrVal);
}

static void api__vfsFSOName(UmkaStackSlot *p, UmkaStackSlot *r) {
  umkaGetResult(p, r)->ptrVal =
      umkaMakeStr(umka, vfsFSOName((FSO *)umkaGetParam(p, 0)->ptrVal));
}

static void api__vfsFSOOpen(UmkaStackSlot *p, UmkaStackSlot *r) {
  umkaGetResult(p, r)->ptrVal = vfsFSOName((FSO *)umkaGetParam(p, 0)->ptrVal);
}

////////////////////////////////////////////////////////////////////////////////

void umkaRuntimeCompile(const char *path) {
  umka = umkaAlloc();
  if (!umka) {
    panic("Failed to allocate Umka context.");
  }

  if (!umkaInit(umka, path, NULL, 1024 * 1024, NULL, 0, NULL, true, false,
                warning)) {
    umkaPrintError();
  }

  gfxInit();

  umkaAddFunc(umka, "api__panic", api__panic);

  umkaAddFunc(umka, "api__asm_in8", api__asm_in8);
  umkaAddFunc(umka, "api__asm_out8", api__asm_out8);
  umkaAddFunc(umka, "api__asm_lidt", api__asm_lidt);
  umkaAddFunc(umka, "api__asm_cli", api__asm_cli);
  umkaAddFunc(umka, "api__asm_sti", api__asm_sti);

  umkaAddFunc(umka, "api__setupTypePtr", api__setupTypePtr);
  umkaAddFunc(umka, "api__getaddr", api__getaddr);
  umkaAddFunc(umka, "api__getptr", api__getptr);
  umkaAddFunc(umka, "api__writes8", api__writes8);
  umkaAddFunc(umka, "api__reads8", api__reads8);
  umkaAddFunc(umka, "api__reads", api__reads);

  umkaAddFunc(umka, "api__gfxSetColor", api__gfxSetColor);
  umkaAddFunc(umka, "api__gfxDrawRect", api__gfxDrawRect);
  umkaAddFunc(umka, "api__gfxDrawClip", api__gfxDrawClip);
  umkaAddFunc(umka, "api__gfxDrawChar", api__gfxDrawChar);
  umkaAddFunc(umka, "api__gfxDrawIcon", api__gfxDrawIcon);
  umkaAddFunc(umka, "api__gfxDrawBackground", api__gfxDrawBackground);
  umkaAddFunc(umka, "api__gfxDims", api__gfxDims);
  umkaAddFunc(umka, "api__gfxSwap", api__gfxSwap);

  umkaAddFunc(umka, "api__vfsRoot", api__vfsRoot);
  umkaAddFunc(umka, "api__vfsNext", api__vfsNext);
  umkaAddFunc(umka, "api__vfsChild", api__vfsChild);
  umkaAddFunc(umka, "api__vfsFSOName", api__vfsFSOName);
  umkaAddFunc(umka, "api__vfsFSOOpen", api__vfsFSOOpen);
  umkaAddFunc(umka, "api__vfsHookFlush", api__vfsHookFlush);

  if (!umkaCompile(umka)) {
    umkaPrintError();
  }

  if (!umkaGetFunc(umka, NULL, "system__register", &umkaRegisterFn)) {
    panic("Function 'system__register' not found.");
  }

  if (!umkaGetFunc(umka, NULL, "system__init", &umkaInitFn)) {
    panic("Function 'system__init' not found.");
  }

  if (!umkaGetFunc(umka, NULL, "system__irq", &umkaIRQFn)) {
    panic("Function 'system__irq' not found.");
  }

  if (!umkaGetFunc(umka, NULL, "system__schedule", &umkaScheduleFn)) {
    panic("Function 'system__schedule' not found.");
  }
}

void umkaRuntimeRegister(const char *name, void *addr) {
  umkaGetParam(umkaRegisterFn.params, 0)->ptrVal = umkaMakeStr(umka, name);
  umkaGetParam(umkaRegisterFn.params, 1)->ptrVal = addr;

  umkaCall(umka, &umkaRegisterFn);
  if (!umkaAlive(umka)) {
    umkaPrintError();
  }
}

void umkaRuntimeInit() {
  umkaCall(umka, &umkaInitFn);
  if (!umkaAlive(umka)) {
    umkaPrintError();
  }
}

static inline void out8(uint16_t port, uint8_t data) {
  asm volatile("outb %0, %1" : : "a"(data), "d"(port));
}

static inline uint8_t in8(uint16_t port) {
  uint8_t data;
  asm volatile("inb %1, %0" : "=a"(data) : "d"(port));
  return data;
}

struct {
  int irq;
  uint8_t bytes[128];
  size_t length;
} typedef Packet;

void packetAddByte(Packet *packet, uint8_t byte) {
  if (packet->length < sizeof(packet->bytes)) {
    packet->bytes[packet->length++] = byte;
  } else {
    panic("Packet overflow.");
  }
}

static Packet packets[128];
static size_t packet_count;

Packet *makePacket(int irq) {
  Packet packet;

  packet.irq = irq;
  packet.length = 0;

  if (packet_count >= sizeof(packets) / sizeof(packets[0])) {
    panic("Packet queue overflow.");
  }

  packets[packet_count++] = packet;

  return &packets[packet_count - 1];
}

void umkaRuntimeHandleIRQ(uint8_t irq) {
  // Umka does not handle the IRQ by itself, instead, it will be delayed,
  // this is because Umka can't handle being ran on multiple threads so far.
  if (umkaLock)
    goto end;

  if (irq == 12 || irq == 1) {
    umkaLock = 1;
    Packet *p = makePacket(irq);

    while ((in8(0x64) & 1) == 1) {
      packetAddByte(p, in8(0x60));
    }
    umkaLock = 0;

    goto irqend;
  }

end:
  while ((in8(0x64) & 1) == 1) {
    in8(0x60);
  }

irqend:
  if (irq >= 8)
    out8(0xA0, 0x20);
  out8(0x20, 0x20);
}

void umkaRuntimeSchedule() {
  if (umkaLock)
    return;

  umkaLock = 1;
  Packet lpackets[128];
  size_t lpacket_count = packet_count;
  memcpy(lpackets, packets, sizeof(packets));
  packet_count = 0;
  umkaLock = 0;

  for (size_t i = 0; i < lpacket_count; i++) {
    Packet packet = lpackets[i];

    if (packet.length == 0) {
      continue;
    }

    umkaGetParam(umkaIRQFn.params, 0)->uintVal = packet.irq;
    umkaGetParam(umkaIRQFn.params, 1)->ptrVal = packet.bytes;
    umkaGetParam(umkaIRQFn.params, 2)->uintVal = packet.length;
    umkaCall(umka, &umkaIRQFn);

    if (!umkaAlive(umka)) {
      umkaPrintError();
    }
  }

  umkaCall(umka, &umkaScheduleFn);
  if (!umkaAlive(umka)) {
    umkaPrintError();
  }
}
