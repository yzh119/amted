/*
 * \file amted/utils.h
 * \brief Utility functions for AMTED server/client.
 */
#ifndef AMTED_UTILS_H_
#define AMTED_UTILS_H_

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif

#define SOCKET_BUFFER_SIZE 2048

void parse_arguments(int argc, char *argv[], char **ip, int *port) {
  // check ip
  int dot_count = 0;
  for (char *c = argv[1]; *c != '\0'; ++c) {
    if (*c == '.') {
      dot_count++;
    } else if (*c < '0' || *c > '9') {
      fprintf(stderr, "Invalid ip address\n");
      exit(-1);
    }
  }
  if (dot_count != 3) {
    fprintf(stderr, "Invalid ip address\n");
    exit(-1);
  }
  *ip = argv[1];
  // check port
  for (char *c = argv[2]; *c != '\0'; ++c) {
    if (*c < '0' || *c > '9') {
      fprintf(stderr, "Invalid port number.\n");
      exit(-1);
    }
  }
  *port = atoi(argv[2]);
}

int get_file_size(FILE *fp) {
  fseek(fp, 0, SEEK_END);
  int sz = ftell(fp);
  rewind(fp);
  return sz;
}

inline int make_socket_non_blocking(int sfd) {
  int flags = fcntl(sfd, F_GETFL, 0);
  if (flags == -1) {
    fprintf(stderr, "Failed to get file status...\n");
    return -1;
  }
  flags |= O_NONBLOCK;
  int s = fcntl(sfd, F_SETFL, flags);
  if (s == -1) {
    fprintf(stderr, "Failed to set file status...");
    return -1;
  }
  return 0;
}

#endif  // AMTED_UTILS_H_
