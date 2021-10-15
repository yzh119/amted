#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <amted/utils.h>
#include <amted/network.h>
#include <amted/cache.h>
#include <amted/event_status.h>

const int cache_size = 256 * 1024 * 1024;
const int max_events = 16;

amted::Cache global_cache(cache_size);

void run_file_server(char *ip, int port) {
  int sock_fd, conn_fd;
  socklen_t len;
  struct sockaddr_in server_addr, cli;
  
  // create socket
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd == 1) {
    fprintf(stderr, "Socket creation failed...\n");
    abort();
  } else {
    printf("Socket creation successful...\n");
  }

  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port);

  // bind socket
  if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
    fprintf(stderr, "Socket binding failed...\n");
    abort();
  } else {
    printf("Socket binding successful...\n");
  }

  if (make_socket_non_blocking(sock_fd) == -1) {
    abort();
  } else {
    printf("Set socket to non-blocking mode.\n");
  }

  // listen
  if (listen(sock_fd, 5) != 0) {
    fprintf(stderr, "Listen failed...\n");
    abort();
  } else {
    printf("Server listening...\n");
  }

  static char buf[SOCKET_BUFFER_SIZE];
  bzero(buf, sizeof(buf));

#ifdef __linux
  struct epoll_event event;
  struct epoll_event *events;

  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    fprintf(stderr, "Failed to create epoll...\n");
    abort();
  } else {
    printf("Epoll creation successful...\n");
  }

  // add sock_fd to epoll set.
  EventStatus* status = new EventStatus;
  status->conn_fd = sock_fd;
  status->code = StatusCode::kAwaitingConncetion;
  event.data.ptr = (void *)status;
  event.events = EPOLLIN | EPOLLET;  // edge-triggered
  
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &event) == -1) {
    fprintf(stderr, "Failed to add event to epoll set...\n");
    abort();
  } else {
    printf("Add socket fd to epoll set...\n");
  }

  // To store the ready list returned by epoll_wait
  events = (struct epoll_event *)calloc(max_events, sizeof(event));

  /* Event loop
   * 1. Check incoming connections in active events.
   * 2. Read request from events.
   * 3. Find filename in global cache.
   * 4. If not hit, load data from global memory (create helper threads).
   * 5. Update cache.
   * 6. Send header to clients.
   * 7. Read file and send data until eof.
   */
  while (1) {
    int n = epoll_wait(epoll_fd, events, max_events, 500);  // wait for 50 milliseconds.
    for (int i = 0; i < n; i++) {
      EventStatus* status = (EventStatus *)events[i].data.ptr;
      if (status == nullptr) {
        fprintf(stderr, "Internal error");
        abort();
      }
      if (is_error_status(events[i].events)) {
        fprintf(stderr, "Epoll status error...\n");
        close(status->conn_fd);
        continue;
      } else if (sock_fd == status->conn_fd) {
        // have a new conncetion.
        struct sockaddr in_addr;
        socklen_t in_len;
        int infd = accept(sock_fd, &in_addr, &in_len);
        if (infd == -1) {
          fprintf(stderr, "Failed to accept conncetion...\n");
          fprintf(stderr, "%d\n", errno);
          abort();
        }
        printf("Accepted connection on descriptor %d...\n", infd);
        if (make_socket_non_blocking(infd) == -1) {
          fprintf(stderr, "Failed to make socket non-blocking...\n");
          abort();
        } else {
          printf("Set the descriptor %d to non-blocking mode...\n", infd);
        }

        // add infd to epoll set.
        EventStatus *status = new EventStatus;
        status->conn_fd = infd;
        status->code = StatusCode::kAwaitingRequest;
        event.data.ptr = (void *)status;
        event.events = EPOLLIN | EPOLLET | EPOLLOUT;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, infd, &event) == -1) {
          fprintf(stderr, "Failed to add event to epoll set...\n");
          abort();
        } else {
          printf("Add event on descriptor %d to epoll set...\n", infd);
        }
      } else {
        // process request
        switch (status->code)
        {
        case StatusCode::kAwaitingRequest:
          ssize_t count;
          count = read(status->conn_fd, buf, sizeof(buf));
          if (count == -1) {
            fprintf(stderr, "Error reading request from descriptor %d", status->conn_fd);
            abort();
          } else if (count == 0) {
            /* End of file. close connection */
            int fd = status->conn_fd;
            printf("Close connection on descriptor %d\n", fd);
            delete (EventStatus *)events[i].data.ptr;
            close(fd);
            continue;
          }
        
          printf("Received request of %s...\n", buf);
          strcpy(status->filename, buf);
          bzero(buf, sizeof(buf));
          status->code = StatusCode::kLoadingFile;
          break;
        case StatusCode::kCacheHit:
          // TODO(zihao)
          break;
        case StatusCode::kCacheMiss:
          // TODO(zihao)
          break;
        case StatusCode::kLoadingFile:
          // TODO(zihao)
          break;
        case StatusCode::kWritingSocket:
          // TODO(zihao)
          break;
        default:
          // No other cases;
          break;
        }
      }
    }
  }
  // TODO
#else
  // single process & blocking
  // main loop
  while (1) {
    // Establish connection;
    conn_fd = accept(sock_fd, (struct sockaddr *)&cli, &len);
    printf("Establish new connection...\n");
    if (conn_fd < 0) {
      fprintf(stderr, "Server accept failed...\n");
      abort();
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
    abort();
  }
  return 0;
}