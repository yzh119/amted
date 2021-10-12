/*
 * \file amted/cache.h
 * \brief Cache data structure that stores files frequently used for AMTED
 * server. Note that the flash paper splits file into chunks while this simplify
 * implementation doesn't.
 */
#ifndef AMTED_CACHE_H_
#define AMTED_CACHE_H_

#include <filesystem>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>

#include "utils.h"

namespace amted {

class File {
 private:
  char* content;
  size_t size;

 public:
  explicit File() : size(0), content(nullptr) {}
  explicit File(std::string path) {
    FILE* fp = fopen(path.c_str(), "r");
    if (fp == nullptr) {
      throw std::runtime_error("File not found.");
    } else {
      size = get_file_size(fp);
      content = new char[this->size];
      fread(content, sizeof(char), this->size, fp);
    }
    fclose(fp);
  }
  ~File() { delete[] content; }

  size_t get_size() const { return size; }

  char* get_content_ptr() const { return content; }
};

class Cache {
 private:
  const size_t cache_size;
  std::unordered_map<std::string, std::tuple<std::shared_ptr<File>, size_t>>
      dict;
  size_t used_size;
  size_t time_stamp;
  std::queue<std::tuple<std::string, size_t>> Q;

  inline std::string absolute_path(std::string path) {
    std::filesystem::path p = path;
    return std::filesystem::absolute(p).string();
  }

  inline void remove_least_recently_used() {
    while (!Q.empty()) {
      std::string p;
      size_t time_stamp_0, time_stamp_1;
      std::shared_ptr<File> f_ptr;
      std::tie(p, time_stamp_0) = Q.front();
      Q.pop();
      std::tie(f_ptr, time_stamp_1) = dict[p];
      if (time_stamp_0 !=
          time_stamp_1) {  // the entry in the queue is out-dated.
        continue;
      } else {
        used_size -= f_ptr->get_size();
        dict.erase(p);
        break;
      }
    }
  }

  inline void update_entry(std::string path, std::shared_ptr<File> f_ptr) {
    Q.push(
        std::make_tuple(path, time_stamp));  // update time stamp in the queue;
    dict[path] =
        std::make_tuple(f_ptr, time_stamp);  // update time stamp in the map;
    time_stamp++;
  }

 public:
  explicit Cache(size_t cache_size)
      : cache_size(cache_size), used_size(0), time_stamp(0) {}
  ~Cache() {}

  std::shared_ptr<File> lookup(std::string path) {
    path = absolute_path(path);
    auto it = dict.find(path);
    if (it != dict.end()) {
      // cache hit
      auto val = it->second;
      std::shared_ptr<File> f_ptr = std::get<0>(val);
      update_entry(path, f_ptr);
      return f_ptr;
    } else {
      // cache miss
      return nullptr;
    }
  }

  inline void add(std::string path, std::shared_ptr<File> f_ptr) {
    path = absolute_path(path);
    // cache miss
    size_t file_size = f_ptr->get_size();
    if (file_size > cache_size) {
      return;  // don't write to cache if file exceed cache size.
    }
    // replacement algorithm
    while (file_size + used_size > cache_size) {
      remove_least_recently_used();
    }
    update_entry(path, f_ptr);
  }
};

}  // namespace amted

#endif  // AMTED_CACHE_H_
