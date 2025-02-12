//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <iostream>

#include "cl_tracer.h"
#include "cl_utils.h"

static ClTracer* tracer = nullptr;

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./cl_tracer[.exe] [options] <application> <args>" <<
    std::endl;
  std::cout << "Options:" << std::endl;
  std::cout <<
    "--call-logging [-c]       Trace host API calls" <<
    std::endl;
  std::cout <<
    "--host-timing  [-h]       Report host API execution time" <<
    std::endl;
  std::cout <<
    "--device-timing [-d]      Report kernels execution time" <<
    std::endl;
  std::cout <<
    "--device-timeline [-t]    Trace device activities" <<
    std::endl;
  std::cout <<
    "--chrome-call-logging     Dump host API calls to JSON file" <<
    std::endl;
  std::cout <<
    "--chrome-device-timeline  Dump device activities to JSON file" <<
    std::endl;
  std::cout <<
    "--chrome-device-stages    Dump device activities by stages to JSON file" <<
    std::endl;
  std::cout <<
    "--tid                     Print thread ID into host API trace" <<
    std::endl;
  std::cout <<
    "--pid                     Print process ID into host API and device activity trace" <<
    std::endl;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
int ParseArgs(int argc, char* argv[]) {
  int app_index = 1;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--call-logging") == 0 ||
        strcmp(argv[i], "-c") == 0) {
      utils::SetEnv("CLT_CallLogging=1");
      ++app_index;
    } else if (strcmp(argv[i], "--host-timing") == 0 ||
               strcmp(argv[i], "-h") == 0) {
      utils::SetEnv("CLT_HostTiming=1");
      ++app_index;
    } else if (strcmp(argv[i], "--device-timing") == 0 ||
               strcmp(argv[i], "-d") == 0) {
      utils::SetEnv("CLT_DeviceTiming=1");
      ++app_index;
    } else if (strcmp(argv[i], "--device-timeline") == 0 ||
               strcmp(argv[i], "-t") == 0) {
      utils::SetEnv("CLT_DeviceTimeline=1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-call-logging") == 0) {
      utils::SetEnv("CLT_ChromeCallLogging=1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-device-timeline") == 0) {
      utils::SetEnv("CLT_ChromeDeviceTimeline=1");
      ++app_index;
    } else if (strcmp(argv[i], "--chrome-device-stages") == 0) {
      utils::SetEnv("CLT_ChromeDeviceStages=1");
      ++app_index;
    } else if (strcmp(argv[i], "--tid") == 0) {
      utils::SetEnv("CLT_Tid=1");
      ++app_index;
    } else if (strcmp(argv[i], "--pid") == 0) {
      utils::SetEnv("CLT_Pid=1");
      ++app_index;
    } else {
      break;
    }
  }

  if (utils::GetEnv("CLT_ChromeDeviceTimeline") == "1" &&
      utils::GetEnv("CLT_ChromeDeviceStages") == "1") {
    std::cout <<
      "[ERROR] Options --chrome-device-timeline and --chrome-device-stages" <<
      " can't be used together, choose one of them" << std::endl;
    return -1;
  }

  return app_index;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void SetToolEnv() {}

static unsigned ReadArgs() {
  std::string value;
  unsigned options = 0;

  value = utils::GetEnv("CLT_CallLogging");
  if (!value.empty() && value == "1") {
    options |= (1 << CLT_CALL_LOGGING);
  }

  value = utils::GetEnv("CLT_HostTiming");
  if (!value.empty() && value == "1") {
    options |= (1 << CLT_HOST_TIMING);
  }

  value = utils::GetEnv("CLT_DeviceTiming");
  if (!value.empty() && value == "1") {
    options |= (1 << CLT_DEVICE_TIMING);
  }

  value = utils::GetEnv("CLT_DeviceTimeline");
  if (!value.empty() && value == "1") {
    options |= (1 << CLT_DEVICE_TIMELINE);
  }

  value = utils::GetEnv("CLT_ChromeCallLogging");
  if (!value.empty() && value == "1") {
    options |= (1 << CLT_CHROME_CALL_LOGGING);
  }

  value = utils::GetEnv("CLT_ChromeDeviceTimeline");
  if (!value.empty() && value == "1") {
    options |= (1 << CLT_CHROME_DEVICE_TIMELINE);
  }

  value = utils::GetEnv("CLT_ChromeDeviceStages");
  if (!value.empty() && value == "1") {
    options |= (1 << CLT_CHROME_DEVICE_STAGES);
  }

  value = utils::GetEnv("CLT_Tid");
  if (!value.empty() && value == "1") {
    options |= (1 << CLT_TID);
  }

  value = utils::GetEnv("CLT_Pid");
  if (!value.empty() && value == "1") {
    options |= (1 << CLT_PID);
  }

  return options;
}

void EnableProfiling() {
  tracer = ClTracer::Create(ReadArgs());
}

void DisableProfiling() {
  if (tracer != nullptr) {
    delete tracer;
  }
}