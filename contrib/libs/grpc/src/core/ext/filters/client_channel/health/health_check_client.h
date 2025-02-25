/*
 *
 * Copyright 2018 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef GRPC_CORE_EXT_FILTERS_CLIENT_CHANNEL_HEALTH_HEALTH_CHECK_CLIENT_H
#define GRPC_CORE_EXT_FILTERS_CLIENT_CHANNEL_HEALTH_HEALTH_CHECK_CLIENT_H

#include <grpc/support/port_platform.h>

#include <atomic>

#include <grpc/grpc.h>
#include <grpc/support/sync.h>

#include "src/core/ext/filters/client_channel/client_channel_channelz.h"
#include "src/core/ext/filters/client_channel/subchannel.h"
#include "src/core/lib/backoff/backoff.h"
#include "src/core/lib/gprpp/orphanable.h"
#include "src/core/lib/gprpp/ref_counted_ptr.h"
#include "src/core/lib/gprpp/sync.h"
#include "src/core/lib/iomgr/call_combiner.h"
#include "src/core/lib/iomgr/closure.h"
#include "src/core/lib/iomgr/polling_entity.h"
#include "src/core/lib/iomgr/timer.h"
#include "src/core/lib/resource_quota/arena.h"
#include "src/core/lib/transport/byte_stream.h"
#include "src/core/lib/transport/metadata_batch.h"
#include "src/core/lib/transport/transport.h"

namespace grpc_core {

class HealthCheckClient : public InternallyRefCounted<HealthCheckClient> {
 public:
  HealthCheckClient(TString service_name,
                    RefCountedPtr<ConnectedSubchannel> connected_subchannel,
                    grpc_pollset_set* interested_parties,
                    RefCountedPtr<channelz::SubchannelNode> channelz_node,
                    RefCountedPtr<ConnectivityStateWatcherInterface> watcher);

  ~HealthCheckClient() override;

  void Orphan() override;

 private:
  // Contains a call to the backend and all the data related to the call.
  class CallState : public Orphanable {
   public:
    CallState(RefCountedPtr<HealthCheckClient> health_check_client,
              grpc_pollset_set* interested_parties);
    ~CallState() override;

    void Orphan() override;

    void StartCall() Y_ABSL_EXCLUSIVE_LOCKS_REQUIRED(&HealthCheckClient::mu_);

   private:
    void Cancel();

    void StartBatch(grpc_transport_stream_op_batch* batch);
    static void StartBatchInCallCombiner(void* arg, grpc_error_handle error);

    void CallEndedLocked(bool retry)
        Y_ABSL_EXCLUSIVE_LOCKS_REQUIRED(health_check_client_->mu_);

    static void OnComplete(void* arg, grpc_error_handle error);
    static void RecvInitialMetadataReady(void* arg, grpc_error_handle error);
    static void RecvMessageReady(void* arg, grpc_error_handle error);
    static void RecvTrailingMetadataReady(void* arg, grpc_error_handle error);
    static void StartCancel(void* arg, grpc_error_handle error);
    static void OnCancelComplete(void* arg, grpc_error_handle error);

    static void OnByteStreamNext(void* arg, grpc_error_handle error);
    void ContinueReadingRecvMessage();
    grpc_error_handle PullSliceFromRecvMessage();
    void DoneReadingRecvMessage(grpc_error_handle error);

    static void AfterCallStackDestruction(void* arg, grpc_error_handle error);

    RefCountedPtr<HealthCheckClient> health_check_client_;
    grpc_polling_entity pollent_;

    Arena* arena_;
    CallCombiner call_combiner_;
    grpc_call_context_element context_[GRPC_CONTEXT_COUNT] = {};

    // The streaming call to the backend. Always non-null.
    // Refs are tracked manually; when the last ref is released, the
    // CallState object will be automatically destroyed.
    SubchannelCall* call_;

    grpc_transport_stream_op_batch_payload payload_;
    grpc_transport_stream_op_batch batch_;
    grpc_transport_stream_op_batch recv_message_batch_;
    grpc_transport_stream_op_batch recv_trailing_metadata_batch_;

    grpc_closure on_complete_;

    // send_initial_metadata
    grpc_metadata_batch send_initial_metadata_;

    // send_message
    ManualConstructor<SliceBufferByteStream> send_message_;

    // send_trailing_metadata
    grpc_metadata_batch send_trailing_metadata_;

    // recv_initial_metadata
    grpc_metadata_batch recv_initial_metadata_;
    grpc_closure recv_initial_metadata_ready_;

    // recv_message
    OrphanablePtr<ByteStream> recv_message_;
    grpc_closure recv_message_ready_;
    grpc_slice_buffer recv_message_buffer_;
    std::atomic<bool> seen_response_{false};

    // True if the cancel_stream batch has been started.
    std::atomic<bool> cancelled_{false};

    // recv_trailing_metadata
    grpc_metadata_batch recv_trailing_metadata_;
    grpc_transport_stream_stats collect_stats_;
    grpc_closure recv_trailing_metadata_ready_;

    // Closure for call stack destruction.
    grpc_closure after_call_stack_destruction_;
  };

  void StartCall();
  void StartCallLocked() Y_ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  void StartRetryTimerLocked() Y_ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  static void OnRetryTimer(void* arg, grpc_error_handle error);

  void SetHealthStatus(grpc_connectivity_state state, const char* reason);
  void SetHealthStatusLocked(grpc_connectivity_state state, const char* reason)
      Y_ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  TString service_name_;
  RefCountedPtr<ConnectedSubchannel> connected_subchannel_;
  grpc_pollset_set* interested_parties_;  // Do not own.
  RefCountedPtr<channelz::SubchannelNode> channelz_node_;
  MemoryAllocator call_allocator_;

  Mutex mu_;
  RefCountedPtr<ConnectivityStateWatcherInterface> watcher_
      Y_ABSL_GUARDED_BY(mu_);
  bool shutting_down_ Y_ABSL_GUARDED_BY(mu_) = false;

  // The data associated with the current health check call.  It holds a ref
  // to this HealthCheckClient object.
  OrphanablePtr<CallState> call_state_ Y_ABSL_GUARDED_BY(mu_);

  // Call retry state.
  BackOff retry_backoff_ Y_ABSL_GUARDED_BY(mu_);
  grpc_timer retry_timer_ Y_ABSL_GUARDED_BY(mu_);
  grpc_closure retry_timer_callback_ Y_ABSL_GUARDED_BY(mu_);
  bool retry_timer_callback_pending_ Y_ABSL_GUARDED_BY(mu_) = false;
};

}  // namespace grpc_core

#endif /* GRPC_CORE_EXT_FILTERS_CLIENT_CHANNEL_HEALTH_HEALTH_CHECK_CLIENT_H */
