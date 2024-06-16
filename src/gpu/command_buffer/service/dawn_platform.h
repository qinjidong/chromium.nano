// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_PLATFORM_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_PLATFORM_H_

#include <dawn/platform/DawnPlatform.h>

#include <memory>

#include "base/time/time.h"
#include "gpu/command_buffer/service/dawn_caching_interface.h"

namespace gpu::webgpu {

class DawnPlatform : public dawn::platform::Platform {
 public:
  explicit DawnPlatform(
      std::unique_ptr<DawnCachingInterface> dawn_caching_interface,
      const char* uma_prefix);
  ~DawnPlatform() override;

  const unsigned char* GetTraceCategoryEnabledFlag(
      dawn::platform::TraceCategory category) override;

  double MonotonicallyIncreasingTime() override;

  uint64_t AddTraceEvent(char phase,
                         const unsigned char* category_group_enabled,
                         const char* name,
                         uint64_t id,
                         double timestamp,
                         int num_args,
                         const char** arg_names,
                         const unsigned char* arg_types,
                         const uint64_t* arg_values,
                         unsigned char flags) override;

  void HistogramCustomCounts(const char* name,
                             int sample,
                             int min,
                             int max,
                             int bucketCount) override;

  void HistogramCustomCountsHPC(const char* name,
                                int sample,
                                int min,
                                int max,
                                int bucketCount) override;

  void HistogramEnumeration(const char* name,
                            int sample,
                            int boundaryValue) override;

  void HistogramSparse(const char* name, int sample) override;

  void HistogramBoolean(const char* name, bool sample) override;

  dawn::platform::CachingInterface* GetCachingInterface() override;

  std::unique_ptr<dawn::platform::WorkerTaskPool> CreateWorkerTaskPool()
      override;

  bool IsFeatureEnabled(dawn::platform::Features feature) override;

 private:
  struct CacheCounts : public base::RefCountedThreadSafe<CacheCounts> {
    CacheCounts();

    std::atomic<bool> did_schedule_log = false;
    std::atomic<uint32_t> cache_miss_count = 0;
    std::atomic<uint32_t> cache_hit_count = 0;

   private:
    friend class base::RefCountedThreadSafe<CacheCounts>;
    ~CacheCounts();
  };

  void HistogramCacheCountHelper(std::string name,
                                 int sample,
                                 int min,
                                 int max,
                                 int bucketCount);

  std::unique_ptr<DawnCachingInterface> dawn_caching_interface_ = nullptr;
  std::string uma_prefix_;
  scoped_refptr<CacheCounts> cache_counts_;
  base::Time startup_time_;
};

}  // namespace gpu::webgpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_PLATFORM_H_