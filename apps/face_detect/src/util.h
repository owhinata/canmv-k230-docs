#ifndef _UTIL_H
#define _UTIL_H
#include <stdint.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

#define ENABLE_PROFILING 0

typedef struct {
  float x;
  float y;
  float w;
  float h;
} box_t;

typedef struct {
  float points[10];
} landmarks_t;

typedef struct {
  int x1;
  int y1;
  int x2;
  int y2;
} face_coordinate;

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
  ScopedTiming(std::string info = "ScopedTiming") : m_info(info) {
    m_start = std::chrono::steady_clock::now();
  }

  ~ScopedTiming() {
    m_stop = std::chrono::steady_clock::now();
    double elapsed_ms =
        std::chrono::duration<double, std::milli>(m_stop - m_start).count();
    std::cout << m_info << " took " << elapsed_ms << " ms" << std::endl;
  }

 private:
  std::string m_info;
  std::chrono::steady_clock::time_point m_start;
  std::chrono::steady_clock::time_point m_stop;
};
#endif
