#include <amted/network.h>
#include <amted/utils.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

typedef std::chrono::high_resolution_clock Time;
typedef std::chrono::milliseconds ms;
typedef std::chrono::duration<float> fsec;

int download_file(int fd, char *filename) {
  static char buf[SOCKET_BUFFER_SIZE];
  bzero(buf, sizeof(buf));
  strcpy(buf, filename);
  while (1) {
    int ret = write(fd, buf, sizeof(buf));  // check buffer overflow.
    if (ret == -1) {
      if (errno == EAGAIN) {
        // try again
      } else {
        fprintf(stderr, "Error sending file requests...\n");
        abort();
      }
    } else {
      break;
    }
  }
  bzero(buf, sizeof(buf));
  printf("Sent download request to server...\n");
  while (1) {
    int ret = read(fd, buf, sizeof(buf));
    if (ret == -1) {
      if (errno == EAGAIN) {
        // try again
        continue;
      } else {
        fprintf(stderr, "Error reading content from socket...\n");
        abort();
      }
    } else {
      break;
    }
  }
  int file_size = std::stoi(buf);
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
  bzero(buf, sizeof(buf));
  int offset = 0, buf_len = 0;
  while (1) {
    bzero(buf, sizeof(buf));
    buf_len = read(fd, buf, std::min<int>(sizeof(buf), file_size - offset));
    if (buf_len == -1) {
      if (errno == EAGAIN) {
        // try again
        continue;
      } else {
        fprintf(stderr, "Error reading content frmo socket...\n");
        abort();
      }
    } else {
      fwrite(buf, sizeof(char), buf_len, fp);
      offset += buf_len;
      if (offset >= file_size) break;
    }
  }
  fclose(fp);
  return 1;
}

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
    if (download_file(sock_fd, filename)) {
      auto toc = Time::now();
      printf("Download successful, total time: %ldms\n",
             std::chrono::duration_cast<ms>(toc - tic).count());
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
