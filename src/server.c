#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <amted/utils.h>
#include <amted/network.h>


void file_server() {
  // TODO(zihao)
}

int main(int argc, char *argv[]) {
  if (argc == 3) {
    char *ip = NULL;
    int port = 0;
    parse_arguments(argc, argv, &ip, &port);
  } else if (argc == 1) {
    printf("IP address and port must be specified, see %s --help for usage\n", argv[0]);
  } else if (argc == 2 && strcmp(argv[1], "--help") == 0) {
    printf("CSE550 AMTED server.\n");
    printf("Usage: %s ip_address port\n", argv[0]);
    printf("ip_address  : \n");
    printf("port        : \n");
  } else {
    fprintf(stderr, "Invalid number of arguments, see %s --help for usage.\n", argv[0]);
    exit(-1);
  }
  return 0;
}