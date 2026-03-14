#ifndef APPS_VEG_CLASSIFY_SRC_UTIL_H_
#define APPS_VEG_CLASSIFY_SRC_UTIL_H_
#include <stdint.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#define ENABLE_PROFILING 0

void read_binary_file(const char *file_name, char *buffer, size_t size = 0);
template <class T>
std::vector<T> read_binary_file(const char *file_name) {
  std::ifstream ifs(file_name, std::ios::binary);
  ifs.seekg(0, ifs.end);
  size_t len = ifs.tellg();
  std::vector<T> vec(len / sizeof(T), 0);
  ifs.seekg(0, ifs.beg);
  ifs.read(reinterpret_cast<char *>(vec.data()), len);
  ifs.close();
  return std::move(vec);
}

class ScopedTiming {
 public:
  explicit ScopedTiming(std::string info = "ScopedTiming") : info_(info) {
    start_ = std::chrono::steady_clock::now();
  }

  ~ScopedTiming() {
    stop_ = std::chrono::steady_clock::now();
    double elapsed_ms =
        std::chrono::duration<double, std::milli>(stop_ - start_).count();
    std::cout << info_ << " took " << elapsed_ms << " ms" << std::endl;
  }

 private:
  std::string info_;
  std::chrono::steady_clock::time_point start_;
  std::chrono::steady_clock::time_point stop_;
};
#endif  // APPS_VEG_CLASSIFY_SRC_UTIL_H_
