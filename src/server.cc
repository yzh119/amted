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
    fprintf(stderr, "Socket creation failed, error code: %d...\n", errno);
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
    fprintf(stderr, "Socket binding failed, error code: %d...\n", errno);
    abort();
  } else {
    printf("Socket binding successful...\n");
  }

  if (make_socket_non_blocking(sock_fd) == -1) {
    fprintf(stderr, "Failed to make socket non-blocking, error code: %d...\n", errno);
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

  // create epoll events
  struct epoll_event event;
  struct epoll_event *events;

  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    fprintf(stderr, "Failed to create epoll, error code: %d...\n", errno);
    abort();
  } else {
    printf("Epoll creation successful...\n");
  }

  // add sock_fd to epoll set.
  {
    EventStatus* status = new EventStatus;
    status->conn_fd = sock_fd;
    status->code = StatusCode::kAwaitingConncetion;
    event.data.ptr = status;
  }
  event.events = EPOLLIN;  // edge-triggered
  
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &event) == -1) {
    fprintf(stderr, "Failed to add event to epoll set, error code: %d...\n", errno);
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
    int n = epoll_wait(epoll_fd, events, max_events, 100);  // wait for 100 milliseconds.
    for (int i = 0; i < n; i++) {
      if (events[i].data.ptr == nullptr) {
        fprintf(stderr, "Internal error");
        abort();
      }
      EventStatus *status_i = (EventStatus *)events[i].data.ptr;
      if (is_error_status(events[i].events)) {
        fprintf(stderr, "Epoll status error...\n");
        close(status_i->conn_fd);
        continue;
      } else if (sock_fd == status_i->conn_fd) {
        // establish new conncetion.
        struct sockaddr in_addr;
        socklen_t in_len = sizeof(in_addr);
        int infd = accept(sock_fd, &in_addr, &in_len);
        if (infd == -1) {
          fprintf(stderr, "Failed to accept conncetion, error code: %d...\n", errno);
          abort();
        }
        printf("Accepted connection on descriptor %d...\n", infd);
        if (make_socket_non_blocking(infd) == -1) {
          fprintf(stderr, "Failed to make socket non-blocking, error code: %d...\n", errno);
          abort();
        } else {
          printf("Set the descriptor %d to non-blocking mode...\n", infd);
        }

        // add infd to epoll set.
        EventStatus *new_status = new EventStatus;
        new_status->conn_fd = infd;
        new_status->code = StatusCode::kAwaitingRequest;
        event.data.ptr = (void *)new_status;
        event.events = EPOLLIN | EPOLLOUT;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, infd, &event) == -1) {
          fprintf(stderr, "Failed to add event to epoll set, error code: %d...\n", errno);
          abort();
        } else {
          printf("Add event on descriptor %d to epoll set...\n", infd);
        }
      } else {
        // process request
        switch (status_i->code)
        {
        case StatusCode::kAwaitingRequest: {
          if (!(events[i].events & EPOLLIN)) {
            break;
          };
          ssize_t s;
          s = read(status_i->conn_fd, buf, sizeof(buf));
          if (s == -1) {
            fprintf(stderr, "Error reading request from descriptor %d, error code %d\n", status_i->conn_fd, errno);
            abort();
          } else if (s == 0) {
            /* End of file. close connection */
            printf("Close connection on descriptor %d\n", status_i->conn_fd);
            close(status_i->conn_fd);
            delete status_i;
            break;
          }
          printf("Received request of %s...\n", buf);
          status_i->filename = buf;
          bzero(buf, sizeof(buf));
          status_i->code = StatusCode::kLoadingFile;
          break;
        }
        case StatusCode::kCacheHit:
          // TODO(zihao)
          break;
        case StatusCode::kCacheMiss:
          // TODO(zihao)
          break;
        case StatusCode::kLoadingFile: {
          std::string filename = status_i->filename;
          printf("Loading file %s from disk...\n", filename.c_str());
          try {
            status_i->fptr = std::make_shared<amted::File>(filename);
            status_i->file_size = status_i->fptr->get_size();
            printf("Load %s complete...\n", filename.c_str());
          }
          catch (const std::runtime_error& err) {
            fprintf(stderr, "Cannot find file %s in the disk...\n", filename.c_str());
          }
          status_i->code = StatusCode::kWritingHeader;
          break;
        }
        case StatusCode::kWritingHeader: {
          if (!(events[i].events & EPOLLOUT)) {
            break;
          };
          bool file_exist = status_i->fptr != nullptr;
          if (file_exist) {
            sprintf(buf, "%d", (int)status_i->file_size);
          } else {
            sprintf(buf, "%d", -1);
          }
          ssize_t s = write(status_i->conn_fd, buf, sizeof(buf));
          if (s == -1) {
            fprintf(stderr, "Failed to write header to descriptor %d, error code: %d...\n", status_i->conn_fd, errno);
            abort();
          } else {
            if (file_exist) {
              status_i->code = StatusCode::kWritingSocket;
            } else {
              status_i->code = StatusCode::kAwaitingRequest;
            }
          }
          break;
        }
        case StatusCode::kWritingSocket: {
          if (!(events[i].events & EPOLLOUT)) {
            break;
          };
          std::string filename = status_i->filename;
          const size_t file_size = status_i->file_size;
          size_t& offset = status_i->offset;
          printf("Writing %s to descriptor %d, remaining %d bytes...\n", filename.c_str(), status_i->conn_fd, (int)status_i->file_size);
          char *ptr = status_i->fptr->get_content_ptr();
          while (offset < file_size) {
            ssize_t s = write(status_i->conn_fd, ptr + offset, std::min<size_t>(file_size - offset, SOCKET_BUFFER_SIZE));
            if (s == -1) {
              if (errno == EAGAIN) {
                continue;
              } else {
                fprintf(stderr, "Failed write file to descriptor %d, error code: %d...\n", status_i->conn_fd, errno);
                abort();
              }
            } else {
              offset += SOCKET_BUFFER_SIZE;
            }
          }
          if (offset >= file_size) {
            printf("Send successful...\n");
            status_i->code = StatusCode::kAwaitingRequest;
          }
          break;
        }
        default:
          fprintf(stderr, "Internal error, encounter state %d...\n", (int)status_i->code);
          abort();
        }
      }  // process request
    }
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
    abort();
  }
  return 0;
}