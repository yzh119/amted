#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <amted/network.h>
#include <amted/utils.h>


int download_file(int fd, char *filename) {
  return 1;
}

void process_request(char *ip, int port) {
  size_t len = 0;
  struct timeval current_time;
  int sock_fd;
  struct sockaddr_in server_addr; 
  // create socket
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd == 1) {
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
    exit(1);
  } else {
    printf("Connected with %s...\n", ip);
  }
  // process file requests one by one
  static char filename[FILENAME_MAX];
  bzero(filename, sizeof(filename));
  while (1) {
    int pos = 0;
    while ((filename[pos++] = getchar()) != '\n');
    filename[pos - 1] = '\0';
    gettimeofday(&current_time, NULL);
    int start_time = current_time.tv_sec;
    printf("Downloading %s on %s\n", filename, ip);
    gettimeofday(&current_time, NULL);
    if (download_file(sock_fd, filename)) {
      int end_time = current_time.tv_sec;
      printf("Download successful, total time: %ds\n", end_time - start_time);
    } else {
      fprintf(stderr, "Download failed...\n");
    }
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
    printf("ip_address  : The IPv4 address of remote host.\n");
    printf("port        : The port to connect to on the remote host.\n");
  } else {
    fprintf(stderr, "Invalid number of arguments, see %s --help for usage.\n", argv[0]);
    exit(1);
  }
  return 0;
}