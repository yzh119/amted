#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <amted/utils.h>
#include <amted/network.h>


void run_file_server(char *ip, int port) {
  int sock_fd, conn_fd;
  socklen_t len;
  struct sockaddr_in server_addr, cli;
  
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd == 1) {
    fprintf(stderr, "Socket creation failed...\n");
    exit(1);
  } else {
    printf("Socket creation successful...\n");
  }

  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port);

  if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
    fprintf(stderr, "Socket binding failed...\n");
    exit(1);
  } else {
    printf("Socket binding successful...\n");
  }

  // Listen
  if (listen(sock_fd, 5) != 0) {
    fprintf(stderr, "Listen failed...\n");
    exit(1);
  } else {
    printf("Server listening...\n");
  }

  static char buf[SOCKET_BUFFER_SIZE];
  bzero(buf, sizeof(buf));
  // Accept packet;
  conn_fd = accept(sock_fd, (struct sockaddr *)&cli, &len);
  if (conn_fd < 0) {
    fprintf(stderr, "Server accept failed...\n");
    exit(1);
  } else {
    char cli_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, (struct sockaddr *)&cli, cli_ip, INET_ADDRSTRLEN);
    printf("Server accept the client from %s...\n", cli_ip);
    while (1) {
      recv(conn_fd, buf, sizeof(buf), 0);
      printf("Received request of %s from %s...\n", buf, cli_ip);
      FILE *fp = fopen(buf, "r");
      if (fp == NULL) {
        fprintf(stderr, "Open file %s failed...\n", buf);
        sprintf(buf, "%d", -1);
        send(conn_fd, buf, sizeof(buf), 0);
        bzero(buf, sizeof(buf));
      } else {
        printf("Found file %s...\n", buf);
        sprintf(buf, "%d", get_file_size(fp));
        send(conn_fd, buf, sizeof(buf), 0);  // TODO(zihao): check
        bzero(buf, sizeof(buf));
        while (fread(buf, sizeof(char), sizeof(buf), fp)) {
          send(conn_fd, buf, sizeof(buf), 0);
          bzero(buf, sizeof(buf));
        }
        fclose(fp);
      }
    }
    close(conn_fd);
  }
  close(sock_fd);
}

int main(int argc, char *argv[]) {
  if (argc == 3) {
    char *ip = NULL;
    int port = 0;
    parse_arguments(argc, argv, &ip, &port);
    run_file_server(ip, port);
  } else if (argc == 1) {
    printf("IP address and port must be specified, see %s --help for usage\n", argv[0]);
  } else if (argc == 2 && strcmp(argv[1], "--help") == 0) {
    printf("CSE550 AMTED server.\n");
    printf("Usage: %s ip_address port\n", argv[0]);
    printf("ip_address  : The IPv4 address this server listens to.\n");
    printf("port        : The port number this server listens to. \n");
  } else {
    fprintf(stderr, "Invalid number of arguments, see %s --help for usage.\n", argv[0]);
    exit(1);
  }
  return 0;
}