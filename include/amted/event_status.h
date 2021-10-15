#ifndef AMTED_EVENT_STATUS_H_
#define AMTED_EVENT_STATUS_H_

#define MAX_FILENAME_SIZE 256

enum class StatusCode {
  kAwaitingConncetion,
  kAwaitingRequest,
  kCacheHit,
  kCacheMiss,
  kLoadingFile,
  kWritingSocket
};

struct EventStatus {
  StatusCode code = StatusCode::kAwaitingConncetion;
  int offset = 0;
  int disk_fd;
  int conn_fd;
  char filename[MAX_FILENAME_SIZE];
  char* fptr = nullptr;
};

#endif  // AMTED_EVENT_STATUS_H_