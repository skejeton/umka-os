#include "alloc.h"
#include <assert.h>

struct MemNode {
  union {
    struct {
      uint32_t check;
      struct MemNode *next;
      size_t size;
    };

    char _padding[16];
  };

  char data[];
} typedef MemNode;

struct {
  MemNode *tail;
  MemNode *free;
} typedef Allocator;

Allocator alloc;

void allocatorInit(void *start) {
  alloc.tail = start;
  alloc.free = 0;

  *alloc.tail = (MemNode){0};
}

void *allocatorRealloc(void *ptr, size_t size) {
  if (ptr == 0) {
    return allocatorAlloc(size);
  }

  MemNode *node = (MemNode *)((char *)ptr - sizeof(MemNode));

  if (node->size >= size) {
    return ptr;
  }

  void *new = allocatorAlloc(size);
  for (size_t i = 0; i < node->size; i++) {
    ((char *)new)[i] = node->data[i];
  }
  allocatorFree(ptr);

  return new;
}

void *allocatorAlloc(size_t size) {
  if (alloc.free) {
    MemNode *node = alloc.free;
    MemNode *prev = 0;
    while (node->size < size) {
      prev = node;
      if (!node->next) {
        goto normal;
      }
      node = node->next;
    }

    if (node->size >= size) {
      if (node == alloc.free) {
        alloc.free = node->next;
      } else {
        assert(prev);
        prev->next = node->next;
      }
      assert(node->check == 0x11FACADE);
      return node->data;
    }
  }

normal:;
  MemNode *node =
      (MemNode *)(((char *)alloc.tail) + alloc.tail->size + sizeof(MemNode));

  *node = (MemNode){0};
  node->check = 0x11FACADE;
  node->size = size + (16 - (size % 16));
  alloc.tail = node;
  return node->data;
}

void allocatorFree(void *ptr) {
  if (ptr == 0)
    return;

  MemNode *node = (MemNode *)((char *)ptr - sizeof(MemNode));
  assert(node->check == 0x11FACADE);

  node->next = alloc.free;
  alloc.free = node;
}
