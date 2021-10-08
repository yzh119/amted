/*
 * \file amted/utils.h
 * \brief Utility functions for AMTED server/client.
 */
#ifndef AMTED_UTILS_H_
#define AMTED_UTILS_H_


#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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


#endif  // AMTED_UTILS_H_