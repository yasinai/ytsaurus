#pragma once

#include <yt/yt/core/misc/public.h>

#include <yt/yt/client/object_client/public.h>

namespace NYT::NNodeTrackerClient {

////////////////////////////////////////////////////////////////////////////////

namespace NProto {

class TNodeStatistics;
class TNodeResources;
class TNodeResourceLimitsOverrides;

class TDiskResources;
class TDiskLocationResources;

class TAddressMap;
class TNodeAddressMap;

class TNodeDescriptor;
class TNodeDirectory;

} // namespace NProto

YT_DEFINE_ERROR_ENUM(
    ((NoSuchNode)        (1600))
    ((InvalidState)      (1601))
    ((NoSuchNetwork)     (1602))
    ((NoSuchRack)        (1603))
    ((NoSuchDataCenter)  (1604))
);

DEFINE_ENUM(EAddressType,
    ((InternalRpc)    (0))
    ((SkynetHttp)     (1))
    ((MonitoringHttp) (2))
);

DEFINE_ENUM(EMaintenanceType,
    // 0 is reserved for None.
    ((Ban)                      (1))
    ((Decommission)             (2))
    ((DisableSchedulerJobs)     (3))
    ((DisableWriteSessions)     (4))
    ((DisableTabletCells)       (5))
);

using TNodeId = ui32;
constexpr TNodeId InvalidNodeId = 0;
constexpr TNodeId MaxNodeId = (1 << 24) - 1; // TNodeId must fit into 24 bits (see TChunkReplica)

using THostId = NObjectClient::TObjectId;
using TRackId = NObjectClient::TObjectId;
using TDataCenterId = NObjectClient::TObjectId;

// Only domain names, without port number.
using TNetworkAddressList = std::vector<std::pair<TString, TString>>;
using TNetworkPreferenceList = std::vector<TString>;

// Network -> host:port.
using TAddressMap = THashMap<TString, TString>;

// Address type (e.g. RPC, HTTP) -> network -> host:port.
using TNodeAddressMap = THashMap<EAddressType, TAddressMap>;

DECLARE_REFCOUNTED_CLASS(TNodeDirectory)
class TNodeDescriptor;

extern const TString DefaultNetworkName;
extern const TNetworkPreferenceList DefaultNetworkPreferences;

using TMaintenanceId = TGuid;
constexpr int MaxMaintenanceCommentLength = 512;

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NNodeTrackerClient
