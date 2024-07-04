#include "vfs.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

struct Node {
  size_t datasize;
  void *data;
  char filename[32];
  struct Node *parent;
  struct Node *child;
  struct Node *sibling;
} typedef Node;

struct {
  Node *root;
} typedef VFS;

struct Stream {
  Node *node;
  void *flushdata;
  void (*flush)(void *, void *, size_t);
  size_t flushindex;
  size_t i;
};

VFS vfs;

const char *vfsBasename(const char *path) {
  const char *base = path;
  for (int i = 0; path[i]; i++) {
    if (path[i] == '/') {
      base = path + i + 1;
    }
  }
  return base;
}

Node *vfsAlloc(Node *node, const char *file) {
  Node *new = calloc(sizeof(Node), 1);
  new->data = malloc(1);
  new->datasize = 0;
  new->parent = node;
  new->child = NULL;
  new->sibling = NULL;
  strcpy(new->filename, file);

  if (node->child == NULL) {
    node->child = new;
  } else {
    Node *child = node->child;
    while (child->sibling) {
      child = child->sibling;
    }
    child->sibling = new;
  }
  return new;
}

void vfsSetData(Node *node, void *data, size_t size) {
  free(node->data);
  node->data = malloc(size);
  memcpy(node->data, data, size);
  node->datasize = size;
}

void vfsClearData(Node *node) { node->datasize = 0; }

void vfsAppendData(Node *node, void *data, size_t size) {
  node->data = realloc(node->data, node->datasize + size);
  memcpy(((uint8_t *)node->data) + node->datasize, data, size);
  node->datasize += size;
}

void vfsFree(Node *node) {
  // free from parent
  if (node->parent) {
    Node *prev = NULL;
    Node *child = node->parent->child;
    while (child) {
      if (child == node) {
        if (prev) {
          prev->sibling = node->sibling;
        } else {
          node->parent->child = node->sibling;
        }
        break;
      }
      prev = child;
      child = child->sibling;
    }
  }

  // free children
  if (node->child) {
    Node *child = node->child;
    while (child) {
      Node *next = child->sibling;
      vfsFree(child);
      child = next;
    }
  }
  free(node);
}

Node *vfsGetPath(const char *path, bool base) {
  char *sub[32];
  size_t sub_idx = 0;

  // split by '/'
  int n = 0;
  for (int i = 0; path[i]; i++) {
    if (path[i] == '/') {
      if (n > 0) {
        sub[sub_idx] = malloc(n + 1);
        sub[sub_idx][n] = 0;
        memcpy(sub[sub_idx++], path + i - n, n);
        n = 0;
      }
    } else {
      n++;
    }
  }

  // last
  if (n > 0) {
    sub[sub_idx] = malloc(n + 1);
    sub[sub_idx][n] = 0;
    memcpy(sub[sub_idx++], path + strlen(path) - n, n);
  }

  if (base && sub_idx >= 1) {
    free(sub[sub_idx - 1]);
    sub[--sub_idx] = NULL;
  }

  // find path
  Node *node = vfs.root;
  for (int i = 0; i < sub_idx; i++) {
    if (strcmp(sub[i], "..") == 0) {
      if (node->parent == NULL) {
        node = NULL;
        goto cleanup;
      }
      node = node->parent;
      continue;
    }

    if (strcmp(sub[i], ".") == 0) {
      continue;
    }

    if (strcmp(sub[i], "") == 0) {
      continue;
    }

    Node *child = node->child;
    while (child) {
      if (strcmp(child->filename, sub[i]) == 0) {
        node = child;
        break;
      }
      child = child->sibling;
    }

    if (child == NULL) {
      node = NULL;
      goto cleanup;
    }
  }

cleanup:
  for (int i = 0; i < sub_idx; i++) {
    free(sub[i]);
  }

  return node;
}

bool vfsExists(const char *path) { return vfsGetPath(path, false) != NULL; }

Node *vfsCreate_(const char *path) {
  Node *node = vfsGetPath(path, false);
  if (node != NULL) {
    return node;
  }

  node = vfsGetPath(path, true);
  if (node == NULL) {
    return NULL;
  }

  Node *new = vfsAlloc(node, vfsBasename(path));

  return new;
}

int vfsCreate(const char *path) { return vfsCreate_(path) ? 0 : -1; }

void vfsInit() {
  // EDGE CASE: Since data is NULL here, do *NOT* bind vfs.root to a flush hook
  vfs.root = calloc(sizeof(Node), 1);
  vfsCreate("$stdin");
  vfsCreate("$stdout");
  vfsCreate("$stderr");
}

Stream *vfsOpen(const char *path, const char *perm) {
  bool write = false;
  for (int i = 0; perm[i]; i++) {
    if (perm[i] == 'w') {
      write = true;
    }
    if (perm[i] == 'a') {
      return NULL;
    }
  }

  Node *node = vfsGetPath(path, false);
  if (node == NULL) {
    if (write) {
      node = vfsCreate_(path);
      if (node == NULL) {
        return NULL;
      }
    } else {
      return NULL;
    }
  }
  if (write) {
    vfsClearData(node);
  }

  Stream *stream = malloc(sizeof(Stream));
  assert(stream);
  *stream = (Stream){0};
  stream->node = node;
  stream->i = 0;

  return stream;
}

void vfsClose(Stream *stream) {
  assert(stream);
  memset(stream, 0, sizeof(Stream));
  free(stream);
}

size_t vfsRead(Stream *stream, void *dest, size_t size) {
  assert(stream);
  size_t readsize = stream->node->datasize - stream->i;
  if (readsize > size) {
    readsize = size;
  }

  memcpy(dest, (uint8_t *)stream->node->data + stream->i, readsize);
  stream->i += readsize;
  return readsize;
}

size_t vfsWrite(Stream *stream, const void *data, size_t size) {
  assert(stream);
  vfsAppendData(stream->node, (void *)data, size);
  stream->i += size;
  vfsFlush(stream);
  return size;
}

void vfsSeek(Stream *stream, size_t pos, size_t whence) {
  assert(stream);
  if (whence == SEEK_SET) {
    stream->i = pos;
  } else if (whence == SEEK_CUR) {
    stream->i += pos;
  } else if (whence == SEEK_END) {
    stream->i = stream->node->datasize + pos;
  }
}

size_t vfsTell(Stream *stream) {
  assert(stream);
  return stream->i;
}

int vfsEof(Stream *stream) {
  assert(stream);
  return stream->i >= stream->node->datasize;
}

void vfsHookFlush(Stream *stream, void *data,
                  void (*flush)(void *, void *, size_t)) {
  assert(stream);
  assert(flush);
  stream->flush = flush;
  stream->flushdata = data;
  stream->flushindex = 0;
}

void vfsFlush(Stream *stream) {
  assert(stream);
  if (stream->flush == NULL) {
    return;
  }

  if (stream->flushindex < stream->i) {

    stream->flush(stream->flushdata,
                  (uint8_t *)stream->node->data + stream->flushindex,
                  stream->i - stream->flushindex);
    stream->flushindex = stream->i;
  }
}

int vfsRemove(const char *path) {
  assert(path);
  Node *node = vfsGetPath(path, false);
  if (node == NULL) {
    return -1;
  }

  vfsFree(node);
  return 0;
}

char *vfsFSOName(FSO *fso) {
  assert(fso);
  if (fso == NULL) {
    return NULL;
  }

  return ((Node *)fso)->filename;
}

Stream *vfsFSOOpen(FSO *fso, const char *perm) {
  assert(fso);
  for (int i = 0; perm[i]; i++) {
    if (perm[i] == 'a') {
      return NULL;
    }
  }

  Stream *stream = malloc(sizeof(Stream));
  *stream = (Stream){0};
  stream->node = (Node *)fso;
  stream->i = 0;

  return stream;
}

FSO *vfsRoot() { return (FSO *)vfs.root; }

FSO *vfsNext(FSO *of) {
  assert(of);
  Node *node = (Node *)of;

  if (node->sibling == NULL) {
    return NULL;
  }

  node = node->sibling;

  if (node->filename[0] == '$') {
    return vfsNext((FSO *)node);
  }

  return (FSO *)node;
}

FSO *vfsChild(FSO *of) {
  assert(of);
  Node *node = (Node *)of;

  if (node->child == NULL) {
    return NULL;
  }

  node = node->child;

  if (node->filename[0] == '$') {
    return vfsNext((FSO *)node);
  }

  return (FSO *)node;
}
