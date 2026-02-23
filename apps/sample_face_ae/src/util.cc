#include "util.h"

#include <math.h>

#include <fstream>
#include <iostream>

void read_binary_file(const char *file_name, char *buffer, size_t size) {
  std::ifstream ifs(file_name, std::ios::binary);
  ifs.seekg(0, ifs.end);
  size_t len = ifs.tellg();
  if (size != 0) {
    len = size;
  }

  ifs.seekg(0, ifs.beg);
  ifs.read(buffer, len);
  ifs.close();
}
