//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UTILS_LOGGER_H_
#define PTI_TOOLS_UTILS_LOGGER_H_

#include <iostream>
#include <fstream>
#include <mutex>

#include <string.h>

#include "pti_assert.h"

class Logger {
 public:
  Logger(const char* filename = nullptr) {
    if (filename != nullptr) {
      file_.open(filename);
      PTI_ASSERT(file_.is_open());
    }
  }

  ~Logger() {
    if (file_.is_open()) {
      file_.close();
    }
  }

  void Log(const char* text) {
    if (file_.is_open()) {
      const std::lock_guard<std::mutex> lock(lock_);
      file_.write(text, strlen(text));
      file_.flush();
    } else {
      std::cerr << text << std::flush;
    }
  }

 private:
  static std::mutex lock_;
  std::ofstream file_;
};

#endif // PTI_TOOLS_UTILS_LOGGER_H_