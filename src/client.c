#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <amted/network.h>
#include <amted/utils.h>


void process_request(char *ip, int port) {
  char *filename = NULL;
  size_t len = 0;
  struct timeval current_time;
  int sock_fd;
  struct sockaddr_in server_addr; 
  // create socket
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd == -1) {
    fprintf(stderr, "Socket creation failed...\n");
  } else {
    printf("Socket creation successful...\n");
  }
  // create connection
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip);
  server_addr.sin_port = htons(port);
  if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
    fprintf(stderr, "Connection with %s failed...\n", ip);
    exit(-1);
  } else {
    printf("Connected with %s...", ip);
  }
  // process file requests one by one
  while (getline(&filename, &len, stdin)) {
    gettimeofday(&current_time, NULL);
    int start_time = current_time.tv_sec;
    printf("Downloading %s on %s\n", filename, ip);
    gettimeofday(&current_time, NULL);
    int end_time = current_time.tv_sec;
    printf("Download successful, total time: %ds\n", end_time - start_time);
    // free filename
    free(filename);
    filename = NULL;
  }
  // close socket
  close(sock_fd);
}


int main(int argc, char *argv[]) {
  if (argc == 3) {
    char *ip = NULL;
    int port = 0;
    parse_arguments(argc, argv, &ip, &port);
    process_request(ip, port);
  } else if (argc == 1) {
    printf("IP address and port must be specified, see %s --help for usage\n", argv[0]);
  } else if (argc == 2 && strcmp(argv[1], "--help") == 0) {
    printf("CSE550 client\n");
    printf("Usage: %s ip_address port\n", argv[0]);
    printf("ip_address  : \n");
    printf("port        : \n");
  } else {
    fprintf(stderr, "Invalid number of arguments, see %s --help for usage.\n", argv[0]);
    exit(-1);
  }
  return 0;
}