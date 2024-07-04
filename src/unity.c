// clang-format off
#include "mock_libc/mock_libc.c"
#include "mock_libc/fmt.c"
#define Node ALLOC_NODE
#include "mock_libc/alloc.c"
#undef Node
#define Node VFS_NODE
#include "mock_libc/vfs.c"
#undef Node
#include "mock_libc/arith.c"
#include "sys/main.c"
#include "sys/font8x8.c"
#include "sys/gfx.c"
#include "sys/umka.c"
#include "sys/panic.c"
// #include "sys/idt.c"
