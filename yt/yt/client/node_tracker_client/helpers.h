#pragma once

#include "public.h"

#include <yt/yt/core/ypath/public.h>

namespace NYT::NNodeTrackerClient {

////////////////////////////////////////////////////////////////////////////////

NYPath::TYPath GetClusterNodesPath();

NYPath::TYPath GetExecNodesPath();

void ValidateMaintenanceComment(const TString& comment);

////////////////////////////////////////////////////////////////////////////////

namespace NProto {

void FormatValue(TStringBuilderBase* builder, const TDiskLocationResources& locationResources, TStringBuf spec);
TString ToString(const TDiskLocationResources& locationResources);

void FormatValue(TStringBuilderBase* builder, const TDiskResources& diskResources, TStringBuf spec);
TString ToString(const TDiskResources& diskResources);

} // namespace NProto

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NNodeTrackerClient
