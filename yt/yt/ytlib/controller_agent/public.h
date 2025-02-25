#pragma once

#include <yt/yt/ytlib/scheduler/public.h>

#include <yt/yt/ytlib/job_tracker_client/public.h>

#include <yt/yt/library/vector_hdrf/job_resources.h>

namespace NYT::NControllerAgent {

////////////////////////////////////////////////////////////////////////////////

namespace NProto {

class TJobMetrics;
class TTreeTaggedJobMetrics;
class TPoolTreeSchedulingTagFilter;
class TPoolTreeSchedulingTagFilters;
class TOperationDescriptor;
class TInitializeOperationResult;
class TPrepareOperationResult;
class TMaterializeOperationResult;
class TReviveOperationResult;
class TCommitOperationResult;

class TJobStatus;
class TJobSpec;

} // namespace NProto

////////////////////////////////////////////////////////////////////////////////

DEFINE_ENUM(EControllerAgentAlertType,
    (UpdateConfig)
    (UnrecognizedConfigOptions)
    (SnapshotLoadingDisabled)
    (UserJobMonitoringLimited)
    (SnapshotBuildingDisabled)
    (ControllerMemoryOverconsumption)
);

DEFINE_ENUM(EControllerState,
    ((Preparing)(0))
    ((Running)(1))
    ((Failing)(2))
    ((Completed)(3))
    ((Failed)(4))
    ((Aborted)(5))
);

////////////////////////////////////////////////////////////////////////////////

using NVectorHdrf::TJobResources;

using NScheduler::TOperationId;
using NScheduler::TJobId;
using NScheduler::EAbortReason;
using NScheduler::EInterruptReason;
using NScheduler::EOperationType;
using NScheduler::EJobType;
using NScheduler::TOperationSpecBasePtr;

////////////////////////////////////////////////////////////////////////////////

using NJobTrackerClient::EJobState;
using NJobTrackerClient::EJobPhase;

////////////////////////////////////////////////////////////////////////////////

struct TCompletedJobSummary;

DECLARE_REFCOUNTED_CLASS(TProgressCounter)

////////////////////////////////////////////////////////////////////////////////

DEFINE_ENUM(EScheduleJobFailReason,
    ((Unknown)                       ( 0))
    ((OperationNotRunning)           ( 1))
    ((NoPendingJobs)                 ( 2))
    ((NotEnoughChunkLists)           ( 3))
    ((NotEnoughResources)            ( 4))
    ((Timeout)                       ( 5))
    ((EmptyInput)                    ( 6))
    ((NoLocalJobs)                   ( 7))
    ((TaskDelayed)                   ( 8))
    ((NoCandidateTasks)              ( 9))
    ((ResourceOvercommit)            (10))
    ((TaskRefusal)                   (11))
    ((JobSpecThrottling)             (12))
    ((IntermediateChunkLimitExceeded)(13))
    ((DataBalancingViolation)        (14))
    ((UnknownNode)                   (15))
    ((UnknownOperation)              (16))
    ((NoAgentAssigned)               (17))
    ((TentativeTreeDeclined)         (18))
    ((NodeBanned)                    (19))
    ((NodeOffline)                   (20))
    ((ControllerThrottling)          (21))
    ((TentativeSpeculativeForbidden) (22))
    ((OperationIsNotAlive)           (23))
    ((NewJobsForbidden)              (24))
    ((NoPendingProbingJobs)          (25))
);

YT_DEFINE_ERROR_ENUM(
    ((AgentCallFailed)                            (4400))
    ((NoOnlineNodeToScheduleJob)                  (4410))
    ((MaterializationFailed)                      (4415))
    ((OperationControllerMemoryLimitExceeded)     (4416))
    ((IncarnationMismatch)                        (4417))
);

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NControllerAgent
