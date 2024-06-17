/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/base/logging.h"
#include "perfetto/base/status.h"
#include "src/trace_redaction/collect_frame_cookies.h"
#include "src/trace_redaction/collect_system_info.h"
#include "src/trace_redaction/collect_timeline_events.h"
#include "src/trace_redaction/filter_ftrace_using_allowlist.h"
#include "src/trace_redaction/filter_packet_using_allowlist.h"
#include "src/trace_redaction/filter_print_events.h"
#include "src/trace_redaction/filter_sched_waking_events.h"
#include "src/trace_redaction/filter_task_rename.h"
#include "src/trace_redaction/find_package_uid.h"
#include "src/trace_redaction/populate_allow_lists.h"
#include "src/trace_redaction/prune_package_list.h"
#include "src/trace_redaction/redact_ftrace_event.h"
#include "src/trace_redaction/redact_sched_switch.h"
#include "src/trace_redaction/redact_task_newtask.h"
#include "src/trace_redaction/remap_scheduling_events.h"
#include "src/trace_redaction/remove_process_free_comm.h"
#include "src/trace_redaction/scrub_ftrace_events.h"
#include "src/trace_redaction/scrub_process_stats.h"
#include "src/trace_redaction/scrub_process_trees.h"
#include "src/trace_redaction/scrub_trace_packet.h"
#include "src/trace_redaction/suspend_resume.h"
#include "src/trace_redaction/trace_redaction_framework.h"
#include "src/trace_redaction/trace_redactor.h"
#include "src/trace_redaction/verify_integrity.h"

namespace perfetto::trace_redaction {

// Builds and runs a trace redactor.
static base::Status Main(std::string_view input,
                         std::string_view output,
                         std::string_view package_name) {
  TraceRedactor redactor;

  // VerifyIntegrity breaks the CollectPrimitive pattern. Instead of writing to
  // the context, its job is to read trace packets and return errors if any
  // packet does not look "correct". This primitive is added first in an effort
  // to detect and react to bad input before other collectors run.
  redactor.emplace_collect<VerifyIntegrity>();

  // Add all collectors.
  redactor.emplace_collect<FindPackageUid>();
  redactor.emplace_collect<CollectTimelineEvents>();
  redactor.emplace_collect<CollectFrameCookies>();
  redactor.emplace_collect<CollectSystemInfo>();

  // Add all builders.
  redactor.emplace_build<PopulateAllowlists>();
  redactor.emplace_build<AllowSuspendResume>();
  redactor.emplace_build<ReduceFrameCookies>();
  redactor.emplace_build<BuildSyntheticThreads>();

  // Add all transforms.
  auto* scrub_packet = redactor.emplace_transform<ScrubTracePacket>();
  scrub_packet->emplace_back<FilterPacketUsingAllowlist>();
  scrub_packet->emplace_back<FilterFrameEvents>();

  auto* scrub_ftrace_events = redactor.emplace_transform<ScrubFtraceEvents>();
  scrub_ftrace_events->emplace_back<FilterFtraceUsingAllowlist>();
  scrub_ftrace_events->emplace_back<FilterPrintEvents>();
  scrub_ftrace_events->emplace_back<FilterSchedWakingEvents>();
  scrub_ftrace_events->emplace_back<FilterTaskRename>();
  scrub_ftrace_events->emplace_back<FilterSuspendResume>();

  // Scrub packets and ftrace events first as they will remove the largest
  // chucks of data from the trace. This will reduce the amount of data that the
  // other primitives need to operate on.
  redactor.emplace_transform<ScrubProcessTrees>();
  redactor.emplace_transform<PrunePackageList>();
  redactor.emplace_transform<ScrubProcessStats>();

  auto* comms_harness = redactor.emplace_transform<RedactSchedSwitchHarness>();
  comms_harness->emplace_transform<ClearComms>();

  auto* redact_ftrace_events = redactor.emplace_transform<RedactFtraceEvent>();
  redact_ftrace_events
      ->emplace_back<RemoveProcessFreeComm::kFieldId, RemoveProcessFreeComm>();

  // By default, the comm value is cleared. However, when thread merging is
  // enabled (kTaskNewtaskFieldNumber + ThreadMergeDropField), the event is
  // dropped, meaning that this primitive was effectivly a no-op. This primitive
  // remains so that removing thread merging won't leak thread names via new
  // task events.
  auto* redact_new_task =
      redact_ftrace_events
          ->emplace_back<RedactTaskNewTask::kFieldId, RedactTaskNewTask>();
  redact_new_task->emplace_back<ClearComms>();

  // This set of transformations will change pids. This will break the
  // connections between pids and the timeline (the synth threads are not in the
  // timeline). If a transformation uses the timeline, it must be before this
  // transformation.
  auto* redact_sched_events = redactor.emplace_transform<RedactFtraceEvent>();
  redact_sched_events->emplace_back<ThreadMergeRemapFtraceEventPid::kFieldId,
                                    ThreadMergeRemapFtraceEventPid>();
  redact_sched_events->emplace_back<ThreadMergeRemapSchedSwitchPid::kFieldId,
                                    ThreadMergeRemapSchedSwitchPid>();
  redact_sched_events->emplace_back<ThreadMergeRemapSchedWakingPid::kFieldId,
                                    ThreadMergeRemapSchedWakingPid>();
  redact_sched_events->emplace_back<
      ThreadMergeDropField::kTaskNewtaskFieldNumber, ThreadMergeDropField>();
  redact_sched_events
      ->emplace_back<ThreadMergeDropField::kSchedProcessFreeFieldNumber,
                     ThreadMergeDropField>();

  Context context;
  context.package_name = package_name;

  return redactor.Redact(input, output, &context);
}

}  // namespace perfetto::trace_redaction

int main(int argc, char** argv) {
  constexpr int kSuccess = 0;
  constexpr int kFailure = 1;
  constexpr int kInvalidArgs = 2;

  if (argc != 4) {
    PERFETTO_ELOG(
        "Invalid arguments: %s <input file> <output file> <package name>",
        argv[0]);
    return kInvalidArgs;
  }

  auto result = perfetto::trace_redaction::Main(argv[1], argv[2], argv[3]);

  if (result.ok()) {
    return kSuccess;
  }

  PERFETTO_ELOG("Unexpected error: %s", result.c_message());
  return kFailure;
}
