//
// A terrible queue for buffers in libuv
//
#include "uv/include/uv.h"
#include <stdlib.h>
typedef void (*uv_queue_change_cb)(int index, int change);
struct uv_queue_s {
  int length;
  uv_buf_t* buffers;
  uv_mutex_t* mutex;
  uv_queue_change_cb onchange;
};
//
// index - where data changed
// change - if negative that is how many were remove AFTER index
//
typedef struct uv_queue_s uv_queue_t;

//
// QUEUE IS NOT THREAD SAFE UNTIL THIS RETURNS
// SERIOUSLY WHO DOES STUFF BEFORE INIT IS DONE
//
int uv_queue_init (uv_queue_t* queue, uv_queue_change_cb* change_cb) {
  queue->buffers = (uv_buf_t*) malloc(0);
  queue->mutex = (uv_mutex_t*) malloc(sizeof(uv_mutex_t));
  queue->onchange = NULL;
  if (!uv_mutex_init(queue->mutex)) {
    return -1;
  }
  return 0;
}

void uv_queue_free (uv_queue_t* queue) {
  //
  // Wait on other threads
  //
  while (uv_mutex_trylock(queue->mutex));
    int i = 0;
    for (i = 0; i < queue->length; i++) {
      free(queue->buffers[i].base);
    }
    free(queue->buffers);
  uv_mutex_destroy(queue->mutex);
}

int uv_queue_push(uv_queue_t* queue, uv_buf_t buffer) {
  while (uv_mutex_trylock(queue->mutex));
    uv_buf_t* buffers = (uv_buf_t*) realloc(queue->buffers, (queue->length + 1) * sizeof(uv_buf_t));
    if (!buffers) {
      return -1;
    }
    queue->buffers[queue->length] = buffer;
    ++queue->length;
  uv_mutex_unlock(queue->mutex);
  return 0;
}

int uv_queue_unshift(uv_queue_t* queue, uv_buf_t* buffer) {
  if (!queue->length) {
    return -1;
  }
  while (uv_mutex_trylock(queue->mutex));
    uv_buf_t* old_buffers = queue->buffers;
    int new_length = queue->length + 1;
    int new_size = new_length * sizeof(uv_buf_t);
    uv_buf_t* buffers = (uv_buf_t*) malloc(new_size);
    if (!buffers) {
      uv_mutex_unlock(queue->mutex);
      return -1;
    }
    memcpy(old_buffers, buffers + sizeof(uv_buf_t), new_size);
    buffers[0] = *buffer;
    queue->buffers = buffers;
    queue->length = new_length;
  uv_mutex_unlock(queue->mutex);
  free(old_buffers);
  return 0;
}

int uv_queue_shift(uv_queue_t* queue, uv_buf_t* destination) {
  if (!queue->length) {
    return -1;
  }
  while (uv_mutex_trylock(queue->mutex));
    uv_buf_t* old_buffers = queue->buffers;
    if (destination) {
      destination = old_buffers;
    }
    int new_length = queue->length - 1;
    int new_size = new_length * sizeof(uv_buf_t*);
    uv_buf_t* buffers = (uv_buf_t*) malloc(new_size);
    if (!buffers) {
      uv_mutex_unlock(queue->mutex);
      return -1;
    }
    memcpy(old_buffers + sizeof(uv_buf_t*), buffers, new_size);
    queue->buffers = buffers;
    queue->length = new_length;
  uv_mutex_unlock(queue->mutex);
  free(old_buffers);
  return 0;
}

void uv_queue_read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf) {
  uv_queue_t* queue = stream->data;
  uv_queue_push(queue, &buf);
}
