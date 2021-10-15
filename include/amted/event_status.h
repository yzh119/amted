#ifndef AMTED_EVENT_STATUS_H_
#define AMTED_EVENT_STATUS_H_

#include <memory>
#include "cache.h"

#define MAX_FILENAME_SIZE 256

enum class StatusCode {
  kAwaitingConncetion,
  kAwaitingRequest,
  kCacheHit,
  kCacheMiss,
  kLoadingFile,
  kWritingHeader,
  kWritingSocket
};

struct EventStatus {
  StatusCode code = StatusCode::kAwaitingConncetion;
  int disk_fd;
  int conn_fd;
  std::string filename;
  std::shared_ptr<amted::File> fptr;
  size_t file_size = 0;
  size_t offset = 0;
};

#endif  // AMTED_EVENT_STATUS_H_