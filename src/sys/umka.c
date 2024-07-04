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
static int umkaRegisterFn;
static int umkaInitFn;
static int umkaIRQFn;
static int umkaScheduleFn;
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
  panic("(Umka) %s\n", (char *)p[0].ptrVal);
}

static void api__asm_in8(UmkaStackSlot *p, UmkaStackSlot *r) {
  uint8_t result;
  uint16_t port = p[0].uintVal;
  asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));

  r->uintVal = result;
}

static void api__asm_out8(UmkaStackSlot *p, UmkaStackSlot *r) {
  uint8_t val = p[0].uintVal;
  uint16_t port = p[1].uintVal;

  asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void api__asm_lidt(UmkaStackSlot *p, UmkaStackSlot *r) {
  uint16_t limit = p[0].uintVal;
  uint32_t base = p[1].uintVal;

  struct {
    uint16_t limit;
    uint32_t base;
  } __attribute__((packed)) idtr = {limit, base};

  asm volatile("lidt %0" : : "m"(idtr));
}

static void api__asm_cli(UmkaStackSlot *p, UmkaStackSlot *r) { asm("cli"); }
static void api__asm_sti(UmkaStackSlot *p, UmkaStackSlot *r) { asm("sti"); }

static void api__setupTypePtr(UmkaStackSlot *p, UmkaStackSlot *r) {
  umkaTypes[p[1].uintVal] = p[0].ptrVal;
}

static void api__getaddr(UmkaStackSlot *p, UmkaStackSlot *r) {
  r->uintVal = p[0].uintVal;
}

static void api__getptr(UmkaStackSlot *p, UmkaStackSlot *r) {
  r->ptrVal = p[0].ptrVal;
}

static void api__writes8(UmkaStackSlot *p, UmkaStackSlot *r) {
  memcpy(p[0].ptrVal, ((UmkaDynArray(uint8_t) *)(&p[1]))->data,
         umkaGetDynArrayLen(&p[1]));
}

static void api__reads8(UmkaStackSlot *p, UmkaStackSlot *r) {
  size_t len = p[0].uintVal;
  UmkaDynArray(uint8_t) *dest =
      umkaAllocData(umka, sizeof(UmkaDynArray(uint8_t)), NULL);

  umkaMakeDynArray(umka, dest, umkaTypes[TYPE_UINT8ARRAY], len);
  memcpy(dest->data, p[1].ptrVal, len);

  r->ptrVal = dest;
}

static void api__reads(UmkaStackSlot *p, UmkaStackSlot *r) {
  size_t len = p[0].uintVal;
  static int i = 0;

  char *str = malloc(len + 1);
  memcpy(str, p[1].ptrVal, len);
  str[len] = '\0';

  r->ptrVal = umkaMakeStr(umka, str);
  free(str);
}

static void api__gfxSetColor(UmkaStackSlot *p, UmkaStackSlot *r) {
  gfxSetColor(p[3].intVal, p[2].intVal, p[1].intVal, p[0].intVal);
}

static void api__gfxDrawBackground(UmkaStackSlot *p, UmkaStackSlot *r) {
  gfxDrawBackground();
}

static void api__gfxDrawRect(UmkaStackSlot *p, UmkaStackSlot *r) {
  gfxDrawRect(p[3].intVal, p[2].intVal, p[1].intVal, p[0].intVal);
}

static void api__gfxDrawClip(UmkaStackSlot *p, UmkaStackSlot *r) {
  gfxDrawClip(p[3].intVal, p[2].intVal, p[1].intVal, p[0].intVal);
}

static void api__gfxDrawIcon(UmkaStackSlot *p, UmkaStackSlot *r) {
  gfxDrawIcon(p[2].intVal, p[1].intVal, p[0].intVal);
}

static void api__gfxDrawChar(UmkaStackSlot *p, UmkaStackSlot *r) {
  gfxDrawChar(p[2].intVal, p[1].intVal, p[0].intVal);
}

static void api__gfxDims(UmkaStackSlot *p, UmkaStackSlot *r) {
  int w, h;
  gfxDims(&w, &h);
  ((UmkaStackSlot *)p[1].ptrVal)->intVal = w;
  ((UmkaStackSlot *)p[0].ptrVal)->intVal = h;
}

static void api__gfxSwap(UmkaStackSlot *p, UmkaStackSlot *r) { gfxSwap(); }

static void umka__flush(void *data, void *buf, size_t size) {
  UmkaStackSlot slots[2] = {0};
  slots[1].ptrVal = buf;
  slots[0].uintVal = size;

  umkaCall(umka, (size_t)data, 2, slots, NULL);
  if (!umkaAlive(umka)) {
    umkaPrintError();
  }
}

static void api__testIndirect(UmkaStackSlot *p, UmkaStackSlot *r) {
  umkaCall(umka, p[0].intVal, 0, NULL, NULL);
}

static void api__vfsHookFlush(UmkaStackSlot *p, UmkaStackSlot *r) {
  vfsHookFlush(p[2].ptrVal, p[0].ptrVal, umka__flush);
}

static void api__vfsRoot(UmkaStackSlot *p, UmkaStackSlot *r) {
  r->uintVal = (size_t)vfsRoot();
}

static void api__vfsNext(UmkaStackSlot *p, UmkaStackSlot *r) {
  r->uintVal = (size_t)vfsNext((FSO *)p[0].ptrVal);
}

static void api__vfsChild(UmkaStackSlot *p, UmkaStackSlot *r) {
  r->uintVal = (size_t)vfsChild((FSO *)p[0].ptrVal);
}

static void api__vfsFSOName(UmkaStackSlot *p, UmkaStackSlot *r) {
  r->ptrVal = umkaMakeStr(umka, vfsFSOName((FSO *)p[0].ptrVal));
}

static void api__vfsFSOOpen(UmkaStackSlot *p, UmkaStackSlot *r) {
  r->ptrVal = vfsFSOName((FSO *)p[0].ptrVal);
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
  umkaAddFunc(umka, "api__testIndirect", api__testIndirect);

  if (!umkaCompile(umka)) {
    umkaPrintError();
  }

  umkaRegisterFn = umkaGetFunc(umka, NULL, "system__register");
  if (umkaRegisterFn == -1) {
    panic("Function 'system__register' not found.");
  }

  umkaInitFn = umkaGetFunc(umka, NULL, "system__init");
  if (umkaInitFn == -1) {
    panic("Function 'system__init' not found.");
  }

  umkaIRQFn = umkaGetFunc(umka, NULL, "system__irq");
  if (umkaIRQFn == -1) {
    panic("Function 'system__irq' not found.");
  }

  umkaScheduleFn = umkaGetFunc(umka, NULL, "system__schedule");
  if (umkaScheduleFn == -1) {
    panic("Function 'system__schedule' not found.");
  }
}

void umkaRuntimeRegister(const char *name, void *addr) {
  UmkaStackSlot params[2];
  params[1].ptrVal = umkaMakeStr(umka, name);
  params[0].ptrVal = addr;

  umkaCall(umka, umkaRegisterFn, 2, params, NULL);
  if (!umkaAlive(umka)) {
    umkaPrintError();
  }
}

void umkaRuntimeInit() {
  umkaCall(umka, umkaInitFn, 0, NULL, NULL);
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

    UmkaStackSlot p[3] = {0};
    p[2].uintVal = packet.irq;
    p[1].ptrVal = packet.bytes;
    p[0].uintVal = packet.length;

    umkaCall(umka, umkaIRQFn, 3, p, NULL);

    if (!umkaAlive(umka)) {
      umkaPrintError();
    }
  }

  umkaCall(umka, umkaScheduleFn, 0, NULL, NULL);
  if (!umkaAlive(umka)) {
    umkaPrintError();
  }
}
