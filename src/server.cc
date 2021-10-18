#include <amted/cache.h>
#include <amted/event_status.h>
#include <amted/network.h>
#include <amted/thread_pool.h>
#include <amted/utils.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <vector>

constexpr int cache_size = 256 * 1024 * 1024;
constexpr int max_events = 100;

// Global buffer
amted::Cache global_cache(cache_size);
// List of file requests.
std::list<std::future<std::tuple<EventStatus *, std::shared_ptr<amted::File>>>>
    file_requests;
// List of write requests.
std::list<EventStatus *> write_requests;
// Thread pool
ThreadPool pool(max_events);

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

  // Event loop
  while (1) {
    // collect triggered events
    int n = epoll_wait(epoll_fd, events, max_events,
                       10);  // non-blocking mode, timeout: 10ms
    // process socket read.
    for (int i = 0; i < n; ++i) {
      struct epoll_event *ev = &events[i];
      EventStatus *st = (EventStatus *)ev->data.ptr;
      if (sock_fd == st->conn_fd) {
        // establish new connections.
        int infd;
        struct sockaddr in_addr;
        socklen_t in_len = sizeof(in_addr);
        while (1) {
          infd = accept(sock_fd, &in_addr, &in_len);
          if (infd == -1) {
            if (errno == EAGAIN) {
              fprintf(stderr, "Read socket full, try again later...\n");
              continue;
            }
            fprintf(stderr, "Failed to accept conncetion, error code: %d...\n",
                    errno);
            abort();
          } else {
            break;
            printf("Read socket successful...\n");
          }
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
        ssize_t ret;
        while (1) {
          ret = read(st->conn_fd, buf, sizeof(buf));
          if (ret == -1) {
            if (errno == EAGAIN) {
              fprintf(stderr, "Read socket full, try again later...\n");
              continue;
            }
            fprintf(stderr,
                    "Error reading request from descriptor %d, error code %d\n",
                    st->conn_fd, errno);
            abort();
          } else {
            break;
          }
        }
        if (ret == 0) {
          // end of file. close connection
          printf("Close connection on descriptor %d\n", st->conn_fd);
          close(st->conn_fd);
          delete st;
          break;
        }
        printf("Received request of %s...\n", buf);
        st->filename = buf;
        bzero(buf, sizeof(buf));

        // lookup cache
        std::shared_ptr<amted::File> fptr = global_cache.lookup(st->filename);
        if (fptr != nullptr) {
          printf("Cache hit...\n");
          st->fptr = fptr;
          st->file_size = fptr->get_size();
          // change event mode to write;
          event.data.ptr = (void *)st;
          event.events = EPOLLOUT | EPOLLET;
          epoll_ctl(epoll_fd, EPOLL_CTL_MOD, st->conn_fd, &event);
        } else {
          printf("Cache miss, loading file %s from disk...\n",
                  st->filename.c_str());
          // pool.enqueue(blocking_read, ev)
          file_requests.emplace_back(pool.enqueue([st]() {
            try {
              return std::make_tuple(
                  st, std::make_shared<amted::File>(st->filename));
            } catch (const std::runtime_error &err) {
              return std::make_tuple(st, std::shared_ptr<amted::File>());
            }
          }));
        }
      } else if (ev->events & EPOLLOUT) {
        // process write requests;
        write_requests.emplace_back(st);
      }
    }

    for (auto it = file_requests.begin(); it != file_requests.end();) {
      auto status = it->wait_for(std::chrono::milliseconds(0));
      if (status == std::future_status::ready) {
        std::shared_ptr<amted::File> fptr;
        EventStatus *st;
        std::tie(st, fptr) = it->get();
        // file is ready
        std::string filename = st->filename;
        printf("Load %s complete...\n", st->filename.c_str());
        // update event status
        if (fptr != nullptr) {
          st->fptr = fptr;
          st->file_size = fptr->get_size();
          // update cache
          global_cache.add(filename, st->fptr);
        }
        // change event mode to write
        event.data.ptr = (void *)st;
        event.events = EPOLLOUT | EPOLLET;
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, st->conn_fd, &event);
        // remove item from list
        // https://stackoverflow.com/questions/596162/can-you-remove-elements-from-a-stdlist-while-iterating-through-it
        file_requests.erase(it++);
        continue;
      }
      it++;
    }

    // process socket write
    for (auto it = write_requests.begin(); it != write_requests.end();) {
      EventStatus *st = *it;
      bool file_exist = st->fptr != nullptr;
      // writing header;
      if (!st->sent_header) {
        bzero(buf, sizeof(buf));
        if (file_exist) {
          sprintf(buf, "%d", (int)st->file_size);
        } else {
          sprintf(buf, "%d", -1);
        }
        ssize_t ret = write(st->conn_fd, buf, sizeof(buf));
        if (ret == -1) {
          if (errno == EAGAIN) {
            fprintf(stderr, "Write socket full, try again later...\n");
            it++;
            continue;
          } else {
            fprintf(
                stderr,
                "Failed to write header to descriptor %d, error code: %d...\n",
                st->conn_fd, errno);
            abort();
          }
        } else {
          // write success
          if (!file_exist) {
            // switch to read mode is file do not exists.
            clear_status(st);
            event.data.ptr = (void *)st;
            event.events = EPOLLIN | EPOLLET;
            epoll_ctl(epoll_fd, EPOLL_CTL_MOD, st->conn_fd, &event);
            // remove item from list
            write_requests.erase(it++);
            continue;
          }
          st->sent_header = true;
        }
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
        // begin
        ssize_t ret =
            write(st->conn_fd, ptr + offset, file_size - offset);
        if (ret == -1) {
          if (errno == EAGAIN) {
            fprintf(stderr, "Write socket full, try again later...\n");
            it++;
            continue;
          } else {
            fprintf(stderr,
                    "Failed write file to descriptor %d, error code: %d...\n",
                    st->conn_fd, errno);
            abort();
          }
        } else {
          offset += ret;
        }
        // end
        if (offset >= file_size) {
          printf("Send successful...\n");
          // switch to read mode.
          clear_status(st);
          event.data.ptr = (void *)st;
          event.events = EPOLLIN | EPOLLET;
          epoll_ctl(epoll_fd, EPOLL_CTL_MOD, st->conn_fd, &event);
          // remove item from list
          write_requests.erase(it++);
          continue;
        }
      }
      it++;
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