#include <stdio.h>
#include "commander.c"

static void
verbose(command_t *self) {
  printf("verbose: enabled\n");
}

static void
required(command_t *self) {
  printf("required: %s\n", self->arg);
}

static void
optional(command_t *self) {
  printf("optional: %s\n", self->arg);
}

int
main(int argc, char **argv){
  command_t cmd;
  command_init(&cmd, argv[0], "0.0.1");
  command_option(&cmd, "-d", "--detached", "spawn a detached process", verbose);
  command_option(&cmd, "-i", "--ipc [arg]", "listen to a pipe", required);
  command_option(&cmd, "-f", "--fd [arg]", "listen and track a file descriptor", required);
  command_option(&cmd, "-a", "--auth [arg]", "fd to get authorization information from", optional);
  command_parse(&cmd, argc, argv);
  printf("additional args:\n");
  for (int i = 0; i < cmd.argc; ++i) {
    printf("  - '%s'\n", cmd.argv[i]);
  }
  command_free(&cmd);
  return 0;
}
