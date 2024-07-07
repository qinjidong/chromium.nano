// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blocklist.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/blocklist_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_id.h"

using content::BrowserThread;

namespace extensions {

namespace {

void CheckOneExtensionState(Blocklist::IsBlocklistedCallback callback,
                            const Blocklist::BlocklistStateMap& state_map) {
  std::move(callback).Run(state_map.empty() ? NOT_BLOCKLISTED
                                            : state_map.begin()->second);
}

void GetMalwareFromBlocklistStateMap(
    Blocklist::GetMalwareIDsCallback callback,
    const Blocklist::BlocklistStateMap& state_map) {
  std::set<ExtensionId> malware;
  for (const auto& state_pair : state_map) {
    // TODO(oleg): UNKNOWN is treated as MALWARE for backwards compatibility.
    // In future GetMalwareIDs will be removed and the caller will have to
    // deal with BLOCKLISTED_UNKNOWN state returned from GetBlocklistedIDs.
    if (state_pair.second == BLOCKLISTED_MALWARE ||
        state_pair.second == BLOCKLISTED_UNKNOWN) {
      malware.insert(state_pair.first);
    }
  }
  std::move(callback).Run(malware);
}

}  // namespace

Blocklist::Observer::Observer(Blocklist* blocklist) : blocklist_(blocklist) {
  blocklist_->AddObserver(this);
}

Blocklist::Observer::~Observer() {
  blocklist_->RemoveObserver(this);
}

Blocklist::Blocklist() {}

Blocklist::~Blocklist() {}

// static
Blocklist* Blocklist::Get(content::BrowserContext* context) {
  return BlocklistFactory::GetForBrowserContext(context);
}

void Blocklist::GetBlocklistedIDs(const std::set<ExtensionId>& ids,
                                  GetBlocklistedIDsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void Blocklist::GetMalwareIDs(const std::set<ExtensionId>& ids,
                              GetMalwareIDsCallback callback) {
  GetBlocklistedIDs(ids, base::BindOnce(&GetMalwareFromBlocklistStateMap,
                                        std::move(callback)));
}

void Blocklist::IsBlocklisted(const ExtensionId& extension_id,
                              IsBlocklistedCallback callback) {
  std::set<ExtensionId> check;
  check.insert(extension_id);
  GetBlocklistedIDs(
      check, base::BindOnce(&CheckOneExtensionState, std::move(callback)));
}

void Blocklist::GetBlocklistStateForIDs(
    GetBlocklistedIDsCallback callback,
    const std::set<ExtensionId>& blocklisted_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::set<ExtensionId> ids_unknown_state;
  BlocklistStateMap extensions_state;
  for (const auto& blocklisted_id : blocklisted_ids) {
    auto cache_it = blocklist_state_cache_.find(blocklisted_id);
    if (cache_it == blocklist_state_cache_.end() ||
        cache_it->second ==
            BLOCKLISTED_UNKNOWN) {  // Do not return UNKNOWN
                                    // from cache, retry request.
      ids_unknown_state.insert(blocklisted_id);
    } else {
      extensions_state[blocklisted_id] = cache_it->second;
    }
  }

  if (ids_unknown_state.empty()) {
    std::move(callback).Run(extensions_state);
  } else {
    // After the extension blocklist states have been downloaded, call this
    // functions again, but prevent infinite cycle in case server is offline
    // or some other reason prevents us from receiving the blocklist state for
    // these extensions.
    RequestExtensionsBlocklistState(
        ids_unknown_state,
        base::BindOnce(&Blocklist::ReturnBlocklistStateMap,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       blocklisted_ids));
  }
}

void Blocklist::ReturnBlocklistStateMap(
    GetBlocklistedIDsCallback callback,
    const std::set<ExtensionId>& blocklisted_ids) {
  BlocklistStateMap extensions_state;
  for (const auto& blocklisted_id : blocklisted_ids) {
    auto cache_it = blocklist_state_cache_.find(blocklisted_id);
    if (cache_it != blocklist_state_cache_.end())
      extensions_state[blocklisted_id] = cache_it->second;
    // If for some reason we still haven't cached the state of this extension,
    // we silently skip it.
  }

  std::move(callback).Run(extensions_state);
}

void Blocklist::RequestExtensionsBlocklistState(
    const std::set<ExtensionId>& ids,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void Blocklist::OnBlocklistStateReceived(const ExtensionId& id,
                                         BlocklistState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blocklist_state_cache_[id] = state;

  // Go through the opened requests and call the callbacks for those requests
  // for which we already got all the required blocklist states.
  auto requests_it = state_requests_.begin();
  while (requests_it != state_requests_.end()) {
    const std::vector<ExtensionId>& ids = requests_it->first;

    bool have_all_in_cache = true;
    for (const auto& id_str : ids) {
      if (!base::Contains(blocklist_state_cache_, id_str)) {
        have_all_in_cache = false;
        break;
      }
    }

    if (have_all_in_cache) {
      std::move(requests_it->second).Run();
      requests_it = state_requests_.erase(requests_it);  // returns next element
    } else {
      ++requests_it;
    }
  }
}

void Blocklist::ResetDatabaseUpdatedListenerForTest() {
  database_updated_subscription_ = {};
}

void Blocklist::ResetBlocklistStateCacheForTest() {
  blocklist_state_cache_.clear();
}

void Blocklist::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

void Blocklist::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

void Blocklist::IsDatabaseReady(DatabaseReadyCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(false);
}

void Blocklist::ObserveNewDatabase() {
  database_updated_subscription_ = {};
}

void Blocklist::NotifyObservers() {
  for (auto& observer : observers_)
    observer.OnBlocklistUpdated();
}

}  // namespace extensions
