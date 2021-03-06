/*! [91/1895] \file client.cc \brief the client for downloading files for CSE
 * 550 assignment 1. \example
 *    ./550client 99.99.99.99 23333
 */
#include <amted/network.h>
#include <amted/utils.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

typedef std::chrono::high_resolution_clock Time;
typedef std::chrono::milliseconds ms;
typedef std::chrono::duration<float> fsec;

// download file with given filename and socket fd.
int download_file(int fd, char *filename) {
  static char buf[SOCKET_BUFFER_SIZE];
  bzero(buf, sizeof(buf));
  strcpy(buf, filename);
  int ret = 0, offset = 0;

  while (1) {
    ret = write(fd, buf + offset,
                sizeof(buf) - offset);  // check buffer overflow.
    if (ret == -1) {
      abort();
    } else {
      offset += ret;
      if (offset >= sizeof(buf)) {
        break;
      }
    }
  }

  if (strcmp(filename, "EOF") == 0) {  // end of file list.
    return 2;
  }

  printf("Sent download %s request to server...\n", buf);
  int file_size = 0;
  offset = 0;
  bzero(buf, sizeof(buf));
  // load header (file_size)
  while (1) {
    ret = read(fd, buf + offset, sizeof(buf) - offset);
    if (ret == -1) {
      abort();
    } else {
      offset += ret;
      if (offset >= sizeof(buf)) {
        break;
      }
    }
  }

  try {
    file_size = std::stoi(buf);
  } catch (std::invalid_argument &e) {
    fprintf(stderr, "Error parsing header...\n");
    for (char *c = buf; c < buf + sizeof(buf); c++) {
      putchar(*c);
    }
    puts("");
    abort();
  }

  if (file_size >= 0) {
    printf("File %s found on server, size %dB...\n", filename, file_size);
  } else {
    fprintf(stderr, "Cannot find %s on server...\n", filename);
    return 0;
  }
  std::string filename_str = std::string(filename);
  std::string basename =
      filename_str.substr(filename_str.find_last_of("/\\") + 1);
  FILE *fp = fopen(basename.c_str(), "w");
  if (fp == NULL) {
    fprintf(stderr, "File %s creation failed...\n", basename.c_str());
    return 0;
  }

  offset = 0;
  // load file content
  while (1) {
    bzero(buf, sizeof(buf));
    ret = read(fd, buf, std::min<int>(sizeof(buf), file_size - offset));
    if (ret == -1) {
      abort();
    } else {
      fwrite(buf, sizeof(char), ret, fp);
      offset += ret;
      if (offset >= file_size) break;
    }
  }
  fclose(fp);
  return 1;
}

// process requests on given ip address and port.
void process_request(char *ip, int port) {
  size_t len = 0;
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
  if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) !=
      0) {
    fprintf(stderr, "Connection with %s failed...\n", ip);
    abort();
  } else {
    printf("Connected with %s...\n", ip);
  }
  // process file requests one by one
  static char filename[FILENAME_MAX];
  bzero(filename, sizeof(filename));
  while (1) {
    int len = 0;
    while ((filename[len++] = getchar()) != '\n')
      ;
    filename[len - 1] = '\0';
    printf("Downloading %s on %s\n", filename, ip);
    auto tic = Time::now();
    int status = download_file(sock_fd, filename);
    if (status == 1) {
      auto toc = Time::now();
      printf("Download successful, total time: %ldms\n",
             std::chrono::duration_cast<ms>(toc - tic).count());
    } else if (status == 0) {
      fprintf(stderr, "Download failed...\n");
    } else {  // status == 2
      printf("All tasks finished...\n");
      break;
    }
  }
  // close socket
  close(sock_fd);
}

// main function
int main(int argc, char *argv[]) {
  if (argc == 3) {
    char *ip = NULL;
    int port = 0;
    parse_arguments(argc, argv, &ip, &port);
    process_request(ip, port);
  } else if (argc == 1) {
    printf("IP address and port must be specified, see %s --help for usage\n",
           argv[0]);
  } else if (argc == 2 && strcmp(argv[1], "--help") == 0) {
    printf("CSE550 client\n");
    printf("Usage: %s ip_address port\n", argv[0]);
    printf("ip_address  : The IPv4 address of remote host.\n");
    printf("port        : The port to connect to on the remote host.\n");
  } else {
    fprintf(stderr, "Invalid number of arguments, see %s --help for usage.\n",
            argv[0]);
    abort();
  }
  return 0;
}