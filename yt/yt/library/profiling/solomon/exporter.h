#pragma once

#include "public.h"
#include "registry.h"
#include "remote.h"

#include <yt/yt/core/concurrency/thread_pool.h>
#include <yt/yt/core/concurrency/async_rw_lock.h>

#include <yt/yt/core/actions/public.h>

#include <yt/yt/core/http/public.h>

#include <yt/yt/core/ytree/yson_serializable.h>
#include <yt/yt/core/ytree/ypath_detail.h>

#include <library/cpp/monlib/encode/format.h>

#include <yt/yt/library/profiling/sensor.h>
#include <yt/yt/library/profiling/producer.h>

namespace NYT::NProfiling {

////////////////////////////////////////////////////////////////////////////////

struct TShardConfig
    : public NYTree::TYsonStruct
{
    std::vector<TString> Filter;

    std::optional<TDuration> GridStep;

    REGISTER_YSON_STRUCT(TShardConfig);

    static void Register(TRegistrar registrar);
};

DEFINE_REFCOUNTED_TYPE(TShardConfig)

////////////////////////////////////////////////////////////////////////////////

struct TSolomonExporterConfig
    : public NYTree::TYsonStruct
{
    TDuration GridStep;

    TDuration LingerTimeout;

    int WindowSize;

    int ThreadPoolSize;

    bool ConvertCountersToRateForSolomon;
    bool RenameConvertedCounters;

    bool ExportSummary;
    bool ExportSummaryAsMax;
    bool ExportSummaryAsAvg;

    bool MarkAggregates;

    bool EnableCoreProfilingCompatibility;

    bool EnableSelfProfiling;

    bool ReportBuildInfo;

    bool ReportRestart;

    TDuration ResponseCacheTtl;

    TDuration ReadDelay;

    std::optional<TString> Host;

    THashMap<TString, TString> InstanceTags;

    THashMap<TString, TShardConfigPtr> Shards;

    TDuration UpdateSensorServiceTreePeriod;

    TShardConfigPtr MatchShard(const TString& sensorName);

    REGISTER_YSON_STRUCT(TSolomonExporterConfig);

    static void Register(TRegistrar registrar);
};

DEFINE_REFCOUNTED_TYPE(TSolomonExporterConfig)

////////////////////////////////////////////////////////////////////////////////

class TSolomonExporter
    : public TRefCounted
{
public:
    explicit TSolomonExporter(
        TSolomonExporterConfigPtr config,
        TSolomonRegistryPtr registry = nullptr);

    void Register(const TString& prefix, const NYT::NHttp::IServerPtr& server);
    void Register(const TString& prefix, const NYT::NHttp::IRequestPathMatcherPtr& handlers);

    // ReadJson is compatibility function for bulatman@.
    //
    // Returns std::nullopt, if exporter is not ready.
    //
    // You should not use this function directly. Use Register() instead.
    std::optional<TString> ReadJson(const TReadOptions& options = {});

    void AttachRemoteProcess(TCallback<TFuture<TSharedRef>()> dumpSensors);

    TSharedRef DumpSensors();

    // There must be at most 1 running exporter per registry.
    void Start();
    void Stop();

    NYTree::IYPathServicePtr GetSensorService();

private:
    const TSolomonExporterConfigPtr Config_;
    const TSolomonRegistryPtr Registry_;

    const NConcurrency::TActionQueuePtr ControlQueue_;
    const NConcurrency::IThreadPoolPtr OffloadThreadPool_;

    TFuture<void> CollectorFuture_;

    NConcurrency::TAsyncReaderWriterLock Lock_;
    std::vector<std::pair<i64, TInstant>> Window_;

    TInstant StartTime_ = TInstant::Now();

    TSpinLock StatusLock_;
    std::optional<TInstant> LastFetch_;
    THashMap<TString, std::optional<TInstant>> LastShardFetch_;

    struct TCacheKey
    {
        std::optional<TString> Shard;
        ::NMonitoring::EFormat Format;
        ::NMonitoring::ECompression Compression;

        TInstant Now;
        TDuration Period;
        std::optional<TDuration> Grid;

        bool operator == (const TCacheKey& other) const = default;

        operator size_t () const;
    };

    TSpinLock CacheLock_;
    THashMap<TCacheKey, TFuture<TSharedRef>> ResponseCache_;

    TEventTimer CollectionStartDelay_;
    TCounter WindowErrors_;
    TCounter ReadDelays_;
    TCounter ResponseCacheHit_, ResponseCacheMiss_;

    struct TRemoteProcess final
    {
        TRemoteProcess(TCallback<TFuture<TSharedRef>()> dumpSensors, TSolomonRegistry* registry)
            : DumpSensors(dumpSensors)
            , Registry(registry)
        { }

        TCallback<TFuture<TSharedRef>()> DumpSensors;
        TRemoteRegistry Registry;
    };

    TSpinLock RemoteProcessLock_;
    THashSet<TIntrusivePtr<TRemoteProcess>> RemoteProcessList_;

    void DoCollect();
    void TransferSensors();

    void HandleIndex(const TString& prefix, const NHttp::IRequestPtr& req, const NHttp::IResponseWriterPtr& rsp);
    void HandleStatus(const NHttp::IRequestPtr& req, const NHttp::IResponseWriterPtr& rsp);

    void HandleDebugSensors(const NHttp::IRequestPtr& req, const NHttp::IResponseWriterPtr& rsp);
    void HandleDebugTags(const NHttp::IRequestPtr& req, const NHttp::IResponseWriterPtr& rsp);

    void HandleShard(
        const std::optional<TString>& name,
        const NHttp::IRequestPtr& req,
        const NHttp::IResponseWriterPtr& rsp);

    void DoHandleShard(
        const std::optional<TString>& name,
        const NHttp::IRequestPtr& req,
        const NHttp::IResponseWriterPtr& rsp);

    void ValidatePeriodAndGrid(std::optional<TDuration> period, std::optional<TDuration> readGridStep, TDuration gridStep);

    TErrorOr<TReadWindow> SelectReadWindow(TInstant now, TDuration period, std::optional<TDuration> readGridStep, TDuration gridStep);

    void CleanResponseCache();

    bool FilterDefaultGrid(const TString& sensorName);

    // For locking.
    friend class TSensorService;
};

DEFINE_REFCOUNTED_TYPE(TSolomonExporter)

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NProfiling
