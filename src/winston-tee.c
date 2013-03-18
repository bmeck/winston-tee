#include <stdio.h>
#include <stdlib.h>
#include <commander.h>

struct winston_tee_s {
  char* endpoint;
  char* auth;
  int* fds;
  int* ipcs;
};
typedef struct winston_tee_s winston_tee_t;

static void auth_option(command_t *self) {
  winston_tee_t* wt = (winston_tee_t*) self->data;
  wt->auth = (char*)self->arg;
  printf("Auth: %s\n", self->arg);
}
static void fd_option(command_t *self) {
  printf("FD: %s\n", self->arg);
}
static void ipc_option(command_t *self) {
  printf("optional: %s\n", self->arg);
}
static void prefix_option(command_t *self) {
  printf("optional: %s\n", self->arg);
}

int
main(int argc, char **argv){
  command_t cmd;
  command_init(&cmd, argv[0], "0.0.1");
  winston_tee_t* wt = (winston_tee_t*)malloc(sizeof(winston_tee_t));
  cmd.data = wt;
  cmd.usage = "[options] <winstond-endpoint>";
  command_option(&cmd, "-a", "--auth [arg]", "fd to get authorization information from", auth_option);
  command_option(&cmd, "-f", "--fd [arg]", "listen and track a file descriptor", fd_option);
  command_option(&cmd, "-i", "--ipc [arg]", "listen to a pipe", ipc_option);
  command_option(&cmd, "-p", "--prefix <arg>", "prefix for all events", prefix_option);
  command_parse(&cmd, argc, argv);
  if (cmd.argc != 1) {
    printf("invalid number of arguments, one and only one winstond-endpoint should be specified\n");
    command_free(&cmd);
    return 1;
  }
  printf("additional args: %s\n", cmd.argv[0]);
  command_free(&cmd);
  return 0;
}
