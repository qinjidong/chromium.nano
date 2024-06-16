// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/decorators/process_priority_aggregator.h"

#include "components/performance_manager/graph/node_attached_data_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/execution_context/execution_context_registry.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"

namespace performance_manager {

class ProcessPriorityAggregatorAccess {
 public:
  using StorageType = decltype(ProcessNodeImpl::process_priority_data_);

  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      ProcessNodeImpl* process_node) {
    return &process_node->process_priority_data_;
  }
};

namespace {

const char kDescriberName[] = "ProcessPriorityAggregator";

class DataImpl : public ProcessPriorityAggregator::Data,
                 public NodeAttachedDataImpl<DataImpl> {
 public:
  using StorageType = ProcessPriorityAggregatorAccess::StorageType;

  struct Traits : public NodeAttachedDataOwnedByNodeType<ProcessNodeImpl> {};

  explicit DataImpl(const ProcessNode* process_node) {}
  ~DataImpl() override {}

  static std::unique_ptr<NodeAttachedData>* GetUniquePtrStorage(
      ProcessNodeImpl* process_node) {
    return ProcessPriorityAggregatorAccess::GetUniquePtrStorage(process_node);
  }
};

}  // namespace

void ProcessPriorityAggregator::Data::Decrement(base::TaskPriority priority) {
  switch (priority) {
    case base::TaskPriority::LOWEST:
#if DCHECK_IS_ON()
      DCHECK_LT(0u, lowest_count_);
      --lowest_count_;
#endif
      return;

    case base::TaskPriority::USER_VISIBLE: {
      DCHECK_LT(0u, user_visible_count_);
      --user_visible_count_;
      return;
    }

    case base::TaskPriority::USER_BLOCKING: {
      DCHECK_LT(0u, user_blocking_count_);
      --user_blocking_count_;
      return;
    }
  }

  NOTREACHED_IN_MIGRATION();
}

void ProcessPriorityAggregator::Data::Increment(base::TaskPriority priority) {
  switch (priority) {
    case base::TaskPriority::LOWEST:
#if DCHECK_IS_ON()
      ++lowest_count_;
#endif
      return;

    case base::TaskPriority::USER_VISIBLE: {
      ++user_visible_count_;
      return;
    }

    case base::TaskPriority::USER_BLOCKING: {
      ++user_blocking_count_;
      return;
    }
  }

  NOTREACHED_IN_MIGRATION();
}

bool ProcessPriorityAggregator::Data::IsEmpty() const {
#if DCHECK_IS_ON()
  if (lowest_count_)
    return false;
#endif
  return user_blocking_count_ == 0 && user_visible_count_ == 0;
}

base::TaskPriority ProcessPriorityAggregator::Data::GetPriority() const {
  if (user_blocking_count_ > 0)
    return base::TaskPriority::USER_BLOCKING;
  if (user_visible_count_ > 0)
    return base::TaskPriority::USER_VISIBLE;
  return base::TaskPriority::LOWEST;
}

// static
ProcessPriorityAggregator::Data* ProcessPriorityAggregator::Data::GetForTesting(
    ProcessNodeImpl* process_node) {
  return DataImpl::Get(process_node);
}

ProcessPriorityAggregator::ProcessPriorityAggregator() = default;

ProcessPriorityAggregator::~ProcessPriorityAggregator() = default;

void ProcessPriorityAggregator::OnPassedToGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
  graph->AddProcessNodeObserver(this);

  auto* registry =
      execution_context::ExecutionContextRegistry::GetFromGraph(graph);
  // We expect the registry to exist before we are passed to the graph.
  DCHECK(registry);
  registry->AddObserver(this);
}

void ProcessPriorityAggregator::OnTakenFromGraph(Graph* graph) {
  auto* registry =
      execution_context::ExecutionContextRegistry::GetFromGraph(graph);
  CHECK(registry);
  registry->RemoveObserver(this);

  graph->RemoveProcessNodeObserver(this);
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
}

base::Value::Dict ProcessPriorityAggregator::DescribeProcessNodeData(
    const ProcessNode* node) const {
  DataImpl* data = DataImpl::Get(ProcessNodeImpl::FromNode(node));
  if (data == nullptr)
    return base::Value::Dict();

  base::Value::Dict ret;
  ret.Set("user_visible_count", static_cast<int>(data->user_visible_count_));
  ret.Set("user_blocking_count", static_cast<int>(data->user_blocking_count_));
#if DCHECK_IS_ON()
  ret.Set("lowest_count", static_cast<int>(data->lowest_count_));
#endif  // DCHECK_IS_ON()
  return ret;
}

void ProcessPriorityAggregator::OnProcessNodeAdded(
    const ProcessNode* process_node) {
  auto* process_node_impl = ProcessNodeImpl::FromNode(process_node);
  DCHECK(!DataImpl::Get(process_node_impl));
}

void ProcessPriorityAggregator::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
#if DCHECK_IS_ON()
  auto* process_node_impl = ProcessNodeImpl::FromNode(process_node);
  DataImpl* data = DataImpl::Get(process_node_impl);
  if (data) {
    DCHECK(data->IsEmpty());
  }
#endif
}

void ProcessPriorityAggregator::OnExecutionContextAdded(
    const execution_context::ExecutionContext* ec) {
  auto* process_node = ProcessNodeImpl::FromNode(ec->GetProcessNode());
  DataImpl* data = DataImpl::GetOrCreate(process_node);
  data->Increment(ec->GetPriorityAndReason().priority());
  // This is a nop if the priority didn't actually change.
  process_node->set_priority(data->GetPriority());
}

void ProcessPriorityAggregator::OnBeforeExecutionContextRemoved(
    const execution_context::ExecutionContext* ec) {
  auto* process_node = ProcessNodeImpl::FromNode(ec->GetProcessNode());
  DataImpl* data = DataImpl::Get(process_node);
  data->Decrement(ec->GetPriorityAndReason().priority());
  // This is a nop if the priority didn't actually change.
  process_node->set_priority(data->GetPriority());
}

void ProcessPriorityAggregator::OnPriorityAndReasonChanged(
    const execution_context::ExecutionContext* ec,
    const PriorityAndReason& previous_value) {
  // If the priority itself didn't change then ignore this notification.
  const PriorityAndReason& new_value = ec->GetPriorityAndReason();
  if (new_value.priority() == previous_value.priority())
    return;

  // Update the distinct frame priority counts, and set the process priority
  // accordingly.
  auto* process_node = ProcessNodeImpl::FromNode(ec->GetProcessNode());
  DataImpl* data = DataImpl::Get(process_node);
  data->Decrement(previous_value.priority());
  data->Increment(new_value.priority());
  // This is a nop if the priority didn't actually change.
  process_node->set_priority(data->GetPriority());
}

}  // namespace performance_manager