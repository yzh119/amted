#include <amted/cache.h>
#include <amted/event_status.h>
#include <amted/network.h>
#include <amted/utils.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

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
  if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) !=
      0) {
    fprintf(stderr, "Socket binding failed, error code: %d...\n", errno);
    abort();
  } else {
    printf("Socket binding successful...\n");
  }

  if (make_socket_non_blocking(sock_fd) == -1) {
    fprintf(stderr, "Failed to make socket non-blocking, error code: %d...\n",
            errno);
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
    EventStatus *status = new EventStatus;
    status->conn_fd = sock_fd;
    event.data.ptr = status;
  }
  event.events = EPOLLIN | EPOLLET;  // edge-triggered

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &event) == -1) {
    fprintf(stderr, "Failed to add event to epoll set, error code: %d...\n",
            errno);
    abort();
  } else {
    printf("Add socket fd to epoll set...\n");
  }

  // To store the ready list returned by epoll_wait
  events = (struct epoll_event *)calloc(max_events, sizeof(event));

  // file requests, stored as a double buffer
  std::vector<struct epoll_event *> file_requests[2];
  int flip = 0;

  /* Event loop */
  while (1) {
    /* collect triggered events */
    int n = epoll_wait(epoll_fd, events, max_events, -1);
    /* process socket read. */
    for (int i = 0; i < n; ++i) {
      struct epoll_event *ev = &events[i];
      EventStatus *st = (EventStatus *)ev->data.ptr;
      if (sock_fd == st->conn_fd) {
        // establish new connections.
        struct sockaddr in_addr;
        socklen_t in_len = sizeof(in_addr);
        int infd = accept(sock_fd, &in_addr, &in_len);
        if (infd == -1) {
          fprintf(stderr, "Failed to accept conncetion, error code: %d...\n",
                  errno);
          abort();
        }
        printf("Accepted connection on descriptor %d...\n", infd);
        if (make_socket_non_blocking(infd) == -1) {
          fprintf(stderr,
                  "Failed to make socket non-blocking, error code: %d...\n",
                  errno);
          abort();
        } else {
          printf("Set the descriptor %d to non-blocking mode...\n", infd);
        }

        // add infd to epoll set.
        EventStatus *new_status = new EventStatus;
        new_status->conn_fd = infd;
        event.data.ptr = (void *)new_status;
        event.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, infd, &event) == -1) {
          fprintf(stderr,
                  "Failed to add event to epoll set, error code: %d...\n",
                  errno);
          abort();
        } else {
          printf("Add event on descriptor %d to epoll set...\n", infd);
        }
      } else if (ev->events & EPOLLIN) {
        // process reading requests;
        ssize_t s;
        s = read(st->conn_fd, buf, sizeof(buf));
        if (s == -1) {
          fprintf(stderr,
                  "Error reading request from descriptor %d, error code %d\n",
                  st->conn_fd, errno);
          abort();
        } else if (s == 0) {
          /* End of file. close connection */
          printf("Close connection on descriptor %d\n", st->conn_fd);
          close(st->conn_fd);
          delete st;
          break;
        }
        printf("Received request of %s...\n", buf);
        st->filename = buf;
        bzero(buf, sizeof(buf));
        file_requests[flip].push_back(ev);
      }
    }

    printf("%d\n", file_requests[flip].size());
    /* disk I/O */
    for (auto &&ev : file_requests[flip]) {
      EventStatus *st = (EventStatus *)(ev->data.ptr);
      std::string filename = st->filename;
      printf("Loading file %s from disk...\n", filename.c_str());
      try {
        st->fptr = std::make_shared<amted::File>(filename);
        st->file_size = st->fptr->get_size();
        printf("Load %s complete...\n", filename.c_str());
      } catch (const std::runtime_error &err) {
        fprintf(stderr, "Cannot find file %s in the disk...\n",
                filename.c_str());
      }
      /* change to write mode */
      event.data.ptr = (void *)st;
      event.events = EPOLLOUT | EPOLLET;
      epoll_ctl(epoll_fd, EPOLL_CTL_MOD, st->conn_fd, &event);
    }
    file_requests[flip].clear();
    flip = !flip;

    /* process socket write */
    for (int i = 0; i < n; ++i) {
      struct epoll_event *ev = &events[i];
      EventStatus *st = (EventStatus *)ev->data.ptr;
      if (ev->events & EPOLLOUT) {
        bool file_exist = st->fptr != nullptr;
        // writing header;
        if (!st->sent_header) {
          if (file_exist) {
            sprintf(buf, "%d", (int)st->file_size);
          } else {
            sprintf(buf, "%d", -1);
            // switch to read mode
            clear_status(st);
            event.data.ptr = (void *)st;
            event.events = EPOLLIN | EPOLLET;
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, st->conn_fd, &event);
          }
          ssize_t s = write(st->conn_fd, buf, sizeof(buf));
          if (s == -1) {
            if (errno == EAGAIN) {
              printf("Write socket full, try again later...\n");
              continue;
            }
            fprintf(
                stderr,
                "Failed to write header to descriptor %d, error code: %d...\n",
                st->conn_fd, errno);
            abort();
          }
          if (file_exist)
            st->sent_header = true;
        }
        // writing content;
        if (file_exist) {
          std::string filename = st->filename;
          size_t file_size = st->file_size;
          size_t &offset = st->offset;
          printf("Writing %s to descriptor %d, remaining %d bytes...\n",
                filename.c_str(), st->conn_fd,
                (int)st->file_size - (int)st->offset);
          char *ptr = st->fptr->get_content_ptr();
          while (offset < file_size) {
            ssize_t s =
                write(st->conn_fd, ptr + offset,
                      std::min<size_t>(file_size - offset, SOCKET_BUFFER_SIZE));
            if (s == -1) {
              if (errno == EAGAIN) {
                printf("Write socket full, try again later...\n");
                continue;
              } else {
                fprintf(stderr,
                        "Failed write file to descriptor %d, error code: %d...\n",
                        st->conn_fd, errno);
                abort();
              }
            } else {
              offset += SOCKET_BUFFER_SIZE;
            }
          }
          if (offset >= file_size) {
            printf("Send successful...\n");
            // switch to read mode.
            clear_status(st);
            event.data.ptr = (void *)st;
            event.events = EPOLLIN | EPOLLET;
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, st->conn_fd, &event);
          }
        }
      }
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
    printf("IP address and port must be specified, see %s --help for usage\n",
           argv[0]);
  } else if (argc == 2 && strcmp(argv[1], "--help") == 0) {
    printf("CSE550 AMTED server.\n");
    printf("Usage: %s ip_address port\n", argv[0]);
    printf("ip_address  : The IPv4 address this server listens to.\n");
    printf("port        : The port number this server listens to. \n");
  } else {
    fprintf(stderr, "Invalid number of arguments, see %s --help for usage.\n",
            argv[0]);
    abort();
  }
  return 0;
}