#ifndef AMTED_EVENT_STATUS_H_
#define AMTED_EVENT_STATUS_H_

struct EventStatus{
  /*
   * 0: wait for file name, next: (1)
   * 1: have file name, next: lookup item in the cache. (2 or 3)
   * 2: cache hit, next: (6)
   * 3: cache not hit, next: (4)
   * 4: loading file, next: (5)
   * 5: load finished, update cache, next: (6)
   * 6: writing to socket, not finished, next: (7 or 8)
   * 7: write finished, ready for next file, next: (1)
   * 8: write finished, close connection.
   */
  int8_t stage;
  int32_t offset;
  char* fptr;
};

#endif  // AMTED_EVENT_STATUS_H_