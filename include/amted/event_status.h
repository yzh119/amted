#ifndef AMTED_EVENT_STATUS_H_
#define AMTED_EVENT_STATUS_H_

#include <memory>
#include "cache.h"

#define MAX_FILENAME_SIZE 256

struct EventStatus {
  int conn_fd;
  std::string filename;
  std::shared_ptr<amted::File> fptr;
  bool sent_header = false;
  size_t file_size = 0;
  size_t offset = 0;
};

void clear_status(EventStatus *st) {
  st->filename = "";
  st->fptr = nullptr;
  st->sent_header = false;
  st->offset = 0;
  st->file_size = 0;
}

#endif  // AMTED_EVENT_STATUS_H_