#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <amted/utils.h>
#include <amted/network.h>
#include <amted/cache.h>


amted::Cache global_cache(512 * 1024 * 1024);

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

#ifdef __linux
  struct epoll_event event;
  struct epoll_event *events;
  // TODO
#else
  while (1) {
    // Accept packet;
    conn_fd = accept(sock_fd, (struct sockaddr *)&cli, &len);
    printf("Establish new connection...\n");
    if (conn_fd < 0) {
      fprintf(stderr, "Server accept failed...\n");
      exit(1);
    } else {
      char cli_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, (struct sockaddr *)&cli, cli_ip, INET_ADDRSTRLEN);
      printf("Server accept the client from %s...\n", cli_ip);
      while (1) {  // receiving a list of filenames from a single address.
        recv(conn_fd, buf, sizeof(buf), 0);
        if (strlen(buf) == 0) {
          // connection closed;
          printf("Connection with %s closed...\n", cli_ip);
          break;
        }
        std::string filename = buf;
        printf("Received request of %s from %s...\n", filename.c_str(), cli_ip);
        try {
          std::shared_ptr<amted::File> f_ptr = global_cache.lookup(filename);
          bool in_cache = f_ptr != nullptr;
          if (in_cache) {
            printf("Cache hit, load %s from cache...\n", filename.c_str());
          } else {
            printf("Cache miss, load %s from disk...\n", filename.c_str());
            f_ptr = std::make_shared<amted::File>(filename);
            global_cache.add(filename, f_ptr);
          }
          printf("Found file %s...\n", filename.c_str());
          bzero(buf, sizeof(buf));
          sprintf(buf, "%d", f_ptr->get_size());
          send(conn_fd, buf, sizeof(buf), 0);
          bzero(buf, sizeof(buf));
          char *pos = f_ptr->get_content_ptr();
          while (pos - f_ptr->get_content_ptr() < f_ptr->get_size()) {
            send(conn_fd, pos, std::min<size_t>(SOCKET_BUFFER_SIZE, f_ptr->get_size() + f_ptr->get_content_ptr() - pos), 0);
            pos = pos + SOCKET_BUFFER_SIZE;
          }
          printf("File %s sent.\n", filename.c_str());
        }
        catch (const std::runtime_error& error)
        {
          fprintf(stderr, "Open file %s failed...\n", filename.c_str());
          bzero(buf, sizeof(buf));
          sprintf(buf, "%d", -1);
          send(conn_fd, buf, sizeof(buf), 0);
          bzero(buf, sizeof(buf));
        }
      }  // while (1)
      close(conn_fd);
    }
  }  // while (1)
#endif
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