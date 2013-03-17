#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include "uv/include/uv.h"
#include "curl/include/curl/curl.h"
#include "uv_queue.c"

uv_loop_t *loop;
CURLM* curl_handle;

//
// Lesson: curl is terrible
//



void curl_perform(uv_poll_t *req, int status, int events) {
  printf("CURL DEMANDS PERFORMING STUFF!\n");
    int running_handles;
    int flags = 0;
    if (events & UV_READABLE) flags |= CURL_CSELECT_IN;
    if (events & UV_WRITABLE) flags |= CURL_CSELECT_OUT;

    curl_multi_socket_action(curl_handle, req->io_watcher.fd, flags, &running_handles);

    char *done_url;

    CURLMsg *message;
    int pending;
    while ((message = curl_multi_info_read(curl_handle, &pending))) {
        switch (message->msg) {
            case CURLMSG_DONE:
                curl_easy_getinfo(message->easy_handle, CURLINFO_EFFECTIVE_URL, &done_url);
                printf("%s DONE\n", done_url);

                curl_multi_remove_handle(curl_handle, message->easy_handle);
                curl_easy_cleanup(message->easy_handle);

                break;
            default:
                fprintf(stderr, "CURLMSG default\n");
                abort();
        }
    }
}
int handle_socket(CURL *easy, curl_socket_t s, int action, void *userp, void *socketp) {
  printf("CURL DEMANDS SOCKET STUFF!\n");
    uv_poll_t *poll_fd;
    if (action == CURL_POLL_IN || action == CURL_POLL_OUT) {
        if (socketp) {
            poll_fd = (uv_poll_t*) socketp;
        }
        else {
            poll_fd = (uv_poll_t*) malloc(sizeof(uv_poll_t));
            uv_poll_init(loop, poll_fd, s);
        }
        curl_multi_assign(curl_handle, s, (void *) poll_fd);
    }

    switch (action) {
        case CURL_POLL_IN:
            uv_poll_start(poll_fd, UV_READABLE, curl_perform);
            break;
        case CURL_POLL_OUT:
            uv_poll_start(poll_fd, UV_WRITABLE, curl_perform);
            break;
        case CURL_POLL_REMOVE:
            if (socketp) {
                uv_poll_stop((uv_poll_t*) socketp);
                uv_close((uv_handle_t*) socketp, (uv_close_cb) free);
                curl_multi_assign(curl_handle, s, NULL);
            }
            break;
        default:
            abort();
    }

    return 0;
}


size_t read_from_queue(char *ptr, size_t size, size_t nmemb, void *userdata) {
  printf("CURL DEMANDS MORE DATA!\n");
  uv_queue_t* queue = (uv_queue_t*) userdata;
  int to_write = size * nmemb;
  int offset = 0;
  while (to_write - offset > 0) {
    if (!queue->length) {
      break;
    }
    uv_buf_t head = queue->buffers[0];
    int remaining_in_buf = head.len - to_write;
    int writing;
    if (remaining_in_buf >= 0) {
      writing = to_write;
      queue->buffers[0] = uv_buf_init(head.base, remaining_in_buf);
      memcpy(head.base, head.base + writing, remaining_in_buf);
      head.base = realloc(head.base, remaining_in_buf);
      head.len = remaining_in_buf;
    }
    else{
      writing = head.len;
      uv_queue_shift(queue, NULL);
      free(head.base);
    }
    memcpy(ptr + offset, head.base, writing);
    offset += writing;
  }
  if (!offset) {
    return CURL_READFUNC_PAUSE;
  }
  return offset;
}

uv_pipe_t* add_upload(CURLM *curl_handle, const char *url, uv_pipe_t* pipe) {
  CURL *handle = curl_easy_init();
  printf("SENDING TO %s\n", url);
  curl_easy_setopt(handle, CURLOPT_READFUNCTION, read_from_queue);
  curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(handle, CURLOPT_READDATA, pipe->data);
  curl_easy_setopt(handle, CURLOPT_URL, url);
  return handle;
}

const char* msg = "test\n";
uv_buf_t alloc_buffer(size_t suggested_size) {
    return uv_buf_init((char*) malloc(suggested_size), suggested_size);
}
void enqueue_msg(uv_timer_t* timer, int status) {
  uv_buf_t buf = alloc_buffer(strlen(msg));
  memcpy(buf.base, msg, strlen(msg));
  uv_queue_t* queue = (uv_queue_t*) timer->data;
  uv_queue_push(queue, buf);
  printf("ADDED MSG with base: %p\n", buf.base);
  int count = 1;
  curl_multi_perform( curl_handle, &count );
}

uv_timer_t* timer;
void setup_interval(uv_queue_t* queue) {
  timer = (uv_timer_t*) malloc(sizeof(uv_timer_t));
  uv_timer_init(loop, timer);
  timer->data = queue;
  uv_timer_start(timer, enqueue_msg, 0, 1e3);
}

int main(int argc, char **argv) {
  loop = uv_default_loop();
  
  if (argc <= 1)
    return 0;

  if (curl_global_init(CURL_GLOBAL_ALL)) {
    fprintf(stderr, "Could not init cURL\n");
    return 1;
  }


  uv_queue_t *queue = (uv_queue_t*) malloc(sizeof(uv_queue_t));
  uv_queue_init(queue, NULL);
  uv_pipe_t *pipe = (uv_pipe_t*) malloc(sizeof(uv_pipe_t));
  uv_pipe_init(loop, pipe, 0);
  pipe->data = queue;
  curl_handle = curl_multi_init();
  curl_multi_setopt(curl_handle, CURLMOPT_SOCKETFUNCTION, handle_socket);
  curl_multi_add_handle(curl_handle, add_upload(curl_handle, argv[1], pipe));
  setup_interval((uv_queue_t*)pipe->data);
  int count = 1;
  curl_multi_perform( curl_handle, &count );

  uv_run(loop, UV_RUN_DEFAULT);
  curl_multi_cleanup(curl_handle);
  return 0;
}
