//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_CL_HOT_KERNELS_CL_KERNEL_COLLECTOR_H_
#define PTI_SAMPLES_CL_HOT_KERNELS_CL_KERNEL_COLLECTOR_H_

#include <list>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "cl_api_tracer.h"
#include "cl_utils.h"

class ClKernelCollector;

struct ClEventData {
  ClKernelCollector* collector;
  std::string kernel_name;
  cl_kernel kernel;
  size_t local_size[3];
};

struct ClKernelInfo {
  uint64_t total_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;
  size_t simd_width;

  bool operator>(const ClKernelInfo& r) const {
    if (total_time != r.total_time) {
      return total_time > r.total_time;
    }
    return call_count > r.call_count;
  }

  bool operator!=(const ClKernelInfo& r) const {
    if (total_time == r.total_time) {
      return call_count != r.call_count;
    }
    return true;
  }
};

struct ClKernelInterval {
  std::string name;
  uint64_t start;
  uint64_t end;
};

using ClKernelInfoMap = std::map<std::string, ClKernelInfo>;
using ClKernelIntervalList = std::vector<ClKernelInterval>;

class ClKernelCollector {
 public: // Interface
  static ClKernelCollector* Create(cl_device_id device) {
    PTI_ASSERT(device != nullptr);

    ClKernelCollector* collector = new ClKernelCollector();
    PTI_ASSERT(collector != nullptr);

    ClApiTracer* tracer = new ClApiTracer(device, Callback, collector);
    if (tracer == nullptr || !tracer->IsValid()) {
      std::cerr << "[WARNING] Unable to create OpenCL tracer " <<
        "for target device" << std::endl;
      if (tracer != nullptr) {
        delete tracer;
        delete collector;
      }
      return nullptr;
    }

    collector->EnableTracing(tracer);
    return collector;
  }

  ~ClKernelCollector() {
    if (tracer_ != nullptr) {
      delete tracer_;
    }
  }

  void DisableTracing() {
    PTI_ASSERT(tracer_ != nullptr);
    bool disabled = tracer_->Disable();
    PTI_ASSERT(disabled);
  }

  const ClKernelInfoMap& GetKernelInfoMap() const {
    return kernel_info_map_;
  }

  const ClKernelIntervalList& GetKernelIntervalList() const {
    return kernel_interval_list_;
  }

  ClKernelCollector(const ClKernelCollector& copy) = delete;
  ClKernelCollector& operator=(const ClKernelCollector& copy) = delete;

  static void PrintKernelsTable(const ClKernelInfoMap& kernel_info_map) {
    std::set< std::pair<std::string, ClKernelInfo>,
              utils::Comparator > sorted_list(
        kernel_info_map.begin(), kernel_info_map.end());

    uint64_t total_duration = 0;
    size_t max_name_length = kKernelLength;
    for (auto& value : sorted_list) {
      total_duration += value.second.total_time;
      if (value.first.size() > max_name_length) {
        max_name_length = value.first.size();
      }
    }

    if (total_duration == 0) {
      return;
    }

    std::cerr << std::setw(max_name_length) << "Kernel" << "," <<
      std::setw(kCallsLength) << "Calls" << "," <<
      std::setw(kSimdLength) << "SIMD" << "," <<
      std::setw(kTimeLength) << "Time (ns)" << "," <<
      std::setw(kPercentLength) << "Time (%)" << "," <<
      std::setw(kTimeLength) << "Average (ns)" << "," <<
      std::setw(kTimeLength) << "Min (ns)" << "," <<
      std::setw(kTimeLength) << "Max (ns)" << std::endl;

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      size_t simd_width = value.second.simd_width;
      uint64_t duration = value.second.total_time;
      uint64_t avg_duration = duration / call_count;
      uint64_t min_duration = value.second.min_time;
      uint64_t max_duration = value.second.max_time;
      float percent_duration = 100.0f * duration / total_duration;
      std::cerr << std::setw(max_name_length) << function << "," <<
        std::setw(kCallsLength) << call_count << "," <<
        std::setw(kSimdLength) << simd_width << "," <<
        std::setw(kTimeLength) << duration << "," <<
        std::setw(kPercentLength) << std::setprecision(2) <<
          std::fixed << percent_duration << "," <<
        std::setw(kTimeLength) << avg_duration << "," <<
        std::setw(kTimeLength) << min_duration << "," <<
        std::setw(kTimeLength) << max_duration << std::endl;
    }
  }

 private: // Implementation Details
  ClKernelCollector() {}

  void EnableTracing(ClApiTracer* tracer) {
    PTI_ASSERT(tracer != nullptr);
    tracer_ = tracer;

    bool set = true;
    set = set && tracer->SetTracingFunction(
        CL_FUNCTION_clCreateCommandQueueWithProperties);
    set = set && tracer->SetTracingFunction(CL_FUNCTION_clCreateCommandQueue);
    set = set && tracer->SetTracingFunction(CL_FUNCTION_clEnqueueNDRangeKernel);
    set = set && tracer->SetTracingFunction(CL_FUNCTION_clEnqueueReadBuffer);
    set = set && tracer->SetTracingFunction(CL_FUNCTION_clEnqueueWriteBuffer);
    PTI_ASSERT(set);

    bool enabled = tracer_->Enable();
    PTI_ASSERT(enabled);
  }

  void AddKernelInfo(std::string name, uint64_t time, size_t simd_width) {
    PTI_ASSERT(!name.empty());
    if (kernel_info_map_.count(name) == 0) {
      kernel_info_map_[name] = {time, time, time, 1, simd_width};
    } else {
      ClKernelInfo& kernel = kernel_info_map_[name];
      kernel.total_time += time;
      if (time > kernel.max_time) {
        kernel.max_time = time;
      }
      if (time < kernel.min_time) {
        kernel.min_time = time;
      }
      kernel.call_count += 1;
      kernel.simd_width = std::max(kernel.simd_width, simd_width);
    }
  }

  void AddKernelInterval(std::string name, uint64_t start, uint64_t end) {
    PTI_ASSERT(!name.empty());
    PTI_ASSERT(start < end);
    kernel_interval_list_.push_back({name, start, end});
  }

 private: // Callbacks
  static void CL_CALLBACK EventNotify(
      cl_event event, cl_int event_status, void* user_data) {
    PTI_ASSERT(event_status == CL_COMPLETE);

    PTI_ASSERT(user_data != nullptr);
    ClEventData* event_data = reinterpret_cast<ClEventData*>(user_data);

    ClKernelCollector* collector = event_data->collector;
    PTI_ASSERT(collector != nullptr);

    cl_command_queue queue = utils::cl::GetCommandQueue(event);
    PTI_ASSERT(queue != nullptr);

    std::string name = event_data->kernel_name;
    PTI_ASSERT(!name.empty());

    cl_ulong started =
      utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_START);
    cl_ulong ended =
      utils::cl::GetEventTimestamp(event, CL_PROFILING_COMMAND_END);
    cl_ulong time = ended - started;
    PTI_ASSERT(time > 0);

    cl_kernel kernel = event_data->kernel;
    cl_device_id device = utils::cl::GetDevice(queue);
    PTI_ASSERT(device != nullptr);

    size_t simd_width = utils::cl::GetSimdWidth(
        device, kernel, event_data->local_size);
    PTI_ASSERT(simd_width > 0);

    cl_int status = clReleaseKernel(kernel);
    PTI_ASSERT(status == CL_SUCCESS);

    collector->AddKernelInfo(name, time, simd_width);
    collector->AddKernelInterval(name, started, ended);

    status = clReleaseEvent(event);
    PTI_ASSERT(status == CL_SUCCESS);

    delete event_data;
  }

  static void OnEnterCreateCommandQueueWithProperties(cl_callback_data* data) {
    PTI_ASSERT(data != nullptr);

    const cl_params_clCreateCommandQueueWithProperties* params =
      reinterpret_cast<const cl_params_clCreateCommandQueueWithProperties*>(
          data->functionParams);
    PTI_ASSERT(params != nullptr);

    cl_queue_properties* props =
      utils::cl::EnableQueueProfiling(*(params->properties));
    *(params->properties) = props;
    data->correlationData[0] = reinterpret_cast<cl_ulong>(props);
  }

  static void OnExitCreateCommandQueueWithProperties(cl_callback_data* data) {
    PTI_ASSERT(data != nullptr);

    cl_queue_properties* props =
      reinterpret_cast<cl_queue_properties*>(data->correlationData[0]);
    PTI_ASSERT(props != nullptr);
    delete[] props;
  }

  static void OnEnterCreateCommandQueue(cl_callback_data* data) {
    PTI_ASSERT(data != nullptr);

    const cl_params_clCreateCommandQueue* params =
      reinterpret_cast<const cl_params_clCreateCommandQueue*>(
          data->functionParams);
    PTI_ASSERT(params != nullptr);
    *(params->properties) |=
      static_cast<unsigned long>(CL_QUEUE_PROFILING_ENABLE);
  }

  static void OnEnterEnqueueNDRangeKernel(cl_callback_data* data) {
    PTI_ASSERT(data != nullptr);

    const cl_params_clEnqueueNDRangeKernel* params =
      reinterpret_cast<const cl_params_clEnqueueNDRangeKernel*>(
          data->functionParams);
    PTI_ASSERT(params != nullptr);

    if (*(params->event) == nullptr) {
      *(params->event) = reinterpret_cast<cl_event*>(data->correlationData);
    }
  }

  static void OnExitEnqueueNDRangeKernel(
      cl_callback_data* data, ClKernelCollector* collector) {
    PTI_ASSERT(data != nullptr);

    const cl_params_clEnqueueNDRangeKernel* params =
      reinterpret_cast<const cl_params_clEnqueueNDRangeKernel*>(
          data->functionParams);
    PTI_ASSERT(params != nullptr);

    cl_int* return_value =
      reinterpret_cast<cl_int*>(data->functionReturnValue);
    if (*return_value == CL_SUCCESS) {
      PTI_ASSERT(*(params->event) != nullptr);
      cl_int status = CL_SUCCESS;

      if (*(params->event) !=
          reinterpret_cast<cl_event*>(data->correlationData)) {
        status = clRetainEvent(**(params->event));
        PTI_ASSERT(status == CL_SUCCESS);
      }

      ClEventData* event_data = new ClEventData;
      PTI_ASSERT(event_data != nullptr);
      cl_kernel kernel = *(params->kernel);
      event_data->collector = collector;
      event_data->kernel_name = utils::cl::GetKernelName(kernel);
      event_data->kernel = kernel;

      event_data->local_size[0] = 1;
      event_data->local_size[1] = 1;
      event_data->local_size[2] = 1;

      if (*(params->localWorkSize) == nullptr) {
        PTI_ASSERT(*(params->commandQueue) != nullptr);
        cl_device_id device = utils::cl::GetDevice(*(params->commandQueue));
        PTI_ASSERT(device != nullptr);
        event_data->local_size[0] =
          utils::cl::GetKernelLocalSize(device, kernel);
      } else {
        PTI_ASSERT(*(params->workDim) <= 3);
        for (cl_uint i = 0; i < *(params->workDim); ++i) {
          event_data->local_size[i] = (*(params->localWorkSize))[i];
        }
      }

      status = clRetainKernel(kernel);
      PTI_ASSERT(status == CL_SUCCESS);
      status = clSetEventCallback(
          **(params->event), CL_COMPLETE, EventNotify, event_data);
      PTI_ASSERT(status == CL_SUCCESS);
    }
  }

  static void Callback(cl_function_id function,
                      cl_callback_data* callback_data,
                      void* user_data) {
    ClKernelCollector* collector =
      reinterpret_cast<ClKernelCollector*>(user_data);
    PTI_ASSERT(collector != nullptr);

    if (function == CL_FUNCTION_clCreateCommandQueueWithProperties) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterCreateCommandQueueWithProperties(callback_data);
      } else {
        OnExitCreateCommandQueueWithProperties(callback_data);
      }
    } else if (function == CL_FUNCTION_clCreateCommandQueue) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterCreateCommandQueue(callback_data);
      }
    } else if (function == CL_FUNCTION_clEnqueueNDRangeKernel) {
      if (callback_data->site == CL_CALLBACK_SITE_ENTER) {
        OnEnterEnqueueNDRangeKernel(callback_data);
      } else {
        OnExitEnqueueNDRangeKernel(callback_data, collector);
      }
    }
  }

 private: // Data
  ClApiTracer* tracer_ = nullptr;

  std::mutex lock_;
  ClKernelInfoMap kernel_info_map_;
  ClKernelIntervalList kernel_interval_list_;

  static const uint32_t kKernelLength = 10;
  static const uint32_t kCallsLength = 12;
  static const uint32_t kSimdLength = 5;
  static const uint32_t kTimeLength = 20;
  static const uint32_t kPercentLength = 10;
};

#endif // PTI_SAMPLES_CL_HOT_KERNELS_CL_KERNEL_COLLECTOR_H_