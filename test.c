#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include "uv/include/uv.h"
#include "curl/include/curl/curl.h"
#include "uv_queue.c"

CURL* curl_handle;
uv_loop_t *loop;

//
// Lesson: curl is terrible
//
size_t read_from_queue(char *ptr, size_t size, size_t nmemb, void *userdata) {
  printf("CURL DEMANDS MORE DATA!\n");
  uv_queue_t* queue = (uv_queue_t*) userdata;
  int to_write = size * nmemb;
  int offset = 0;
  while (to_write > 0) {
    uv_buf_t head = queue->buffers[0];
    if (!queue->length) {
      break;
    }
    int remaining = head.len - to_write;
    int writing;
    if (remaining >= 0) {
      writing = head.len;
      uv_queue_shift(queue, NULL);
    }
    else{
      writing = to_write;
      queue->buffers[0] = uv_buf_init(head.base, remaining);
      free(head.base);
    }
    memcpy(head.base, ptr + offset, writing);
    offset += writing;
  }
  if (!offset) {
    return CURL_READFUNC_PAUSE;
  }
  return offset;
}

void curl_attach_upload(uv_poll_t *req, int status, int events) {
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

                curl_multi_remove_handle(curl_handle, message->easy_handle);
                curl_easy_cleanup(message->easy_handle);

                break;
            default:
                abort();
        }
    }
}

uv_pipe_t* add_upload(CURLM *curl_handle, const char *url) {
  CURL *handle = curl_easy_init();
  uv_queue_t *queue = (uv_queue_t*) malloc(sizeof(uv_queue_t));
  uv_pipe_t *pipe = (uv_pipe_t*) malloc(sizeof(uv_pipe_t));
  uv_queue_init(queue, NULL);
  pipe->data = queue;
  //
  // Just staple the curl and pipe together, lol
  //
  curl_easy_setopt(handle, CURLOPT_READDATA, queue);
  curl_easy_setopt(handle, CURLOPT_READFUNCTION, read_from_queue);
  curl_easy_setopt(handle, CURLOPT_URL, url);
  curl_multi_add_handle(curl_handle, handle);
  return pipe;
}

int handle_socket(CURL *easy, curl_socket_t s, int action, void *userp, void *socketp) {
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
            break;
        case CURL_POLL_OUT:
            uv_poll_start(poll_fd, UV_WRITABLE, curl_attach_upload);
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


char* msg = "test\n";
void enqueue_msg(uv_timer_t* timer, int status) {
  uv_queue_t* queue = (uv_queue_t*) timer->data;
  uv_buf_t buf = uv_buf_init(msg,5);
  printf("%s READY\n",msg);
  uv_queue_push(queue, buf);
  printf("%s ADDED\n",msg);
}

uv_timer_t* timer;
void setup_interval(uv_queue_t* queue) {
  timer = (uv_timer_t*) malloc(sizeof(uv_timer_t));
  uv_timer_init(loop, timer);
  timer->data = queue;
  uv_timer_start(timer, enqueue_msg, 0, 1e4);
}

int main(int argc, char **argv) {
    loop = uv_default_loop();

    printf("ARGC TEST\n");
    if (argc <= 1)
        return 0;

    if (curl_global_init(CURL_GLOBAL_ALL)) {
        fprintf(stderr, "Could not init cURL\n");
        return 1;
    }

    printf("CURL SETUP STARTING\n");
    curl_handle = curl_multi_init();
    curl_multi_setopt(curl_handle, CURLMOPT_SOCKETFUNCTION, handle_socket);

    printf("PIPE SETUP STARTING\n");
    uv_pipe_t* pipe = add_upload(curl_handle, argv[0]);
    printf("PIPE SETUP DONE\n");
    setup_interval((uv_queue_t*)pipe->data);
    printf("INTERVAL SETUP DONE\n");

    uv_run(loop, UV_RUN_DEFAULT);
    curl_multi_cleanup(curl_handle);
    return 0;
}
