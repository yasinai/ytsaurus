#include <yt/yt/core/rpc/unittests/lib/common.h>

namespace NYT::NRpc {
namespace {

using namespace NYT::NBus;
using namespace NYT::NRpc::NBus;
using namespace NYT::NYTree;
using namespace NYT::NYson;
using namespace NConcurrency;
using namespace NCrypto;
using namespace NYTAlloc;

////////////////////////////////////////////////////////////////////////////////

class TNonExistingServiceProxy
    : public TProxyBase
{
public:
    DEFINE_RPC_PROXY(TNonExistingServiceProxy, NonExistingService);

    DEFINE_RPC_PROXY_METHOD(NMyRpc, DoNothing);
};

class TMyIncorrectProtocolVersionProxy
    : public TProxyBase
{
public:
    DEFINE_RPC_PROXY(TMyIncorrectProtocolVersionProxy, MyService,
        .SetProtocolVersion(2));

    DEFINE_RPC_PROXY_METHOD(NMyRpc, SomeCall);
};

////////////////////////////////////////////////////////////////////////////////

TString StringFromSharedRef(const TSharedRef& sharedRef)
{
    return TString(sharedRef.Begin(), sharedRef.Begin() + sharedRef.Size());
}

////////////////////////////////////////////////////////////////////////////////

template <class TImpl>
using TRpcTest = TTestBase<TImpl>;
template <class TImpl>
using TNotUdsTest = TTestBase<TImpl>;
template <class TImpl>
using TNotGrpcTest = TTestBase<TImpl>;
template <class TImpl>
using TGrpcTest = TTestBase<TImpl>;
TYPED_TEST_SUITE(TRpcTest, TAllTransports);
TYPED_TEST_SUITE(TNotUdsTest, TWithoutUds);
TYPED_TEST_SUITE(TNotGrpcTest, TWithoutGrpc);
TYPED_TEST_SUITE(TGrpcTest, TGrpcOnly);

////////////////////////////////////////////////////////////////////////////////

TYPED_TEST(TRpcTest, Send)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.SomeCall();
    req->set_a(42);
    auto rspOrError = req->Invoke().Get();
    EXPECT_TRUE(rspOrError.IsOK()) << ToString(rspOrError);
    const auto& rsp = rspOrError.Value();
    EXPECT_EQ(142, rsp->b());
}

TYPED_TEST(TRpcTest, RetryingSend)
{
    auto config = New<TRetryingChannelConfig>();
    config->Load(ConvertTo<INodePtr>(TYsonString(TStringBuf(
        "{retry_backoff_time=10}"))));

    IChannelPtr channel = CreateRetryingChannel(
        std::move(config),
        this->CreateChannel());

    {
        TMyProxy proxy(channel);
        auto req = proxy.FlakyCall();
        auto rspOrError = req->Invoke().Get();
        EXPECT_TRUE(rspOrError.IsOK()) << ToString(rspOrError);
    }

    // Channel must be asynchronously deleted after response handling finished.
    // In particular, all possible cyclic dependencies must be resolved.
    WaitForPredicate([&channel] {
        return channel->GetRefCount() == 1;
    });
}

TYPED_TEST(TRpcTest, UserTag)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.PassCall();
    req->SetUser("test-user");
    req->SetUserTag("test-user-tag");
    auto rspOrError = req->Invoke().Get();
    EXPECT_TRUE(rspOrError.IsOK()) << ToString(rspOrError);
    const auto& rsp = rspOrError.Value();
    EXPECT_EQ(req->GetUser(), rsp->user());
    EXPECT_EQ(req->GetUserTag(), rsp->user_tag());
}

////////////////////////////////////////////////////////////////////////////////

TYPED_TEST(TNotUdsTest, Address)
{
    auto testChannel = [] (IChannelPtr channel) {
        TMyProxy proxy(std::move(channel));
        auto req = proxy.SomeCall();
        req->set_a(42);
        auto rspOrError = req->Invoke().Get();
        EXPECT_TRUE(rspOrError.IsOK()) << ToString(rspOrError);
        const auto& rsp = rspOrError.Value();
        EXPECT_FALSE(rsp->GetAddress().empty());
    };

    testChannel(this->CreateChannel());

    {
        auto config = New<TRetryingChannelConfig>();
        config->Load(ConvertTo<INodePtr>(TYsonString(TStringBuf(
            "{retry_backoff_time=10}"))));
        testChannel(CreateRetryingChannel(
            std::move(config),
            this->CreateChannel()));
    }
}

////////////////////////////////////////////////////////////////////////////////

TYPED_TEST(TNotGrpcTest, SendSimple)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.PassCall();
    req->SetUser("test-user");
    req->SetMutationId(TGuid::Create());
    req->SetRetry(true);
    auto rspOrError = req->Invoke().Get();
    EXPECT_TRUE(rspOrError.IsOK()) << ToString(rspOrError);
    const auto& rsp = rspOrError.Value();
    EXPECT_EQ(req->GetUser(), rsp->user());
    EXPECT_FALSE(rsp->has_user_tag());
    EXPECT_EQ(req->GetMutationId(), NYT::FromProto<TMutationId>(rsp->mutation_id()));
    EXPECT_EQ(true, rsp->retry());
}

TYPED_TEST(TNotGrpcTest, StreamingEcho)
{
    TMyProxy proxy(this->CreateChannel());
    proxy.SetDefaultRequestCodec(NCompression::ECodec::Lz4);
    proxy.SetDefaultResponseCodec(NCompression::ECodec::Zstd_1);
    proxy.SetDefaultEnableLegacyRpcCodecs(false);

    const int AttachmentCount = 30;
    const ssize_t AttachmentSize = 2_MB;

    std::mt19937 randomGenerator;
    std::uniform_int_distribution<char> distribution(std::numeric_limits<char>::min(), std::numeric_limits<char>::max());

    std::vector<TSharedRef> attachments;

    for (int i = 0; i < AttachmentCount; ++i) {
        auto data = TSharedMutableRef::Allocate(AttachmentSize);
        for (size_t j = 0; j < AttachmentSize; ++j) {
            data[j] = distribution(randomGenerator);
        }
        attachments.push_back(std::move(data));
    }

    for (bool delayed : {false, true}) {
        auto req = proxy.StreamingEcho();
        req->set_delayed(delayed);
        req->SetResponseHeavy(true);
        auto asyncInvokeResult = req->Invoke();

        std::vector<TSharedRef> receivedAttachments;

        for (const auto& sentData : attachments) {
            WaitFor(req->GetRequestAttachmentsStream()->Write(sentData))
                .ThrowOnError();

            if (!delayed) {
                auto receivedData = WaitFor(req->GetResponseAttachmentsStream()->Read())
                    .ValueOrThrow();
                receivedAttachments.push_back(std::move(receivedData));
            }
        }

        auto asyncCloseResult = req->GetRequestAttachmentsStream()->Close();
        EXPECT_FALSE(asyncCloseResult.IsSet());

        if (delayed) {
            for (int i = 0; i < AttachmentCount; ++i) {
                auto receivedData = WaitFor(req->GetResponseAttachmentsStream()->Read())
                    .ValueOrThrow();
                ASSERT_TRUE(receivedData);
                receivedAttachments.push_back(std::move(receivedData));
            }
        }

        {
            auto receivedData = WaitFor(req->GetResponseAttachmentsStream()->Read())
                .ValueOrThrow();
            ASSERT_FALSE(receivedData);
        }

        for (int i = 0; i < AttachmentCount; ++i) {
            EXPECT_TRUE(TRef::AreBitwiseEqual(attachments[i], receivedAttachments[i]));
        }

        WaitFor(asyncCloseResult)
            .ThrowOnError();

        auto rsp = WaitFor(asyncInvokeResult)
            .ValueOrThrow();

        EXPECT_EQ(AttachmentCount * AttachmentSize, rsp->total_size());
    }
}

TYPED_TEST(TNotGrpcTest, ClientStreamsAborted)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.StreamingEcho();
    req->SetTimeout(TDuration::MilliSeconds(100));

    auto rspOrError = WaitFor(req->Invoke());
    EXPECT_EQ(NYT::EErrorCode::Timeout, rspOrError.GetCode());

    EXPECT_THROW({
        WaitFor(req->GetRequestAttachmentsStream()->Write(TSharedMutableRef::Allocate(100)))
            .ThrowOnError();
    }, TErrorException);

    EXPECT_THROW({
        WaitFor(req->GetResponseAttachmentsStream()->Read())
            .ThrowOnError();
    }, TErrorException);
}

TYPED_TEST(TNotGrpcTest, ServerStreamsAborted)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.ServerStreamsAborted();
    req->SetTimeout(TDuration::MilliSeconds(100));

    auto rspOrError = WaitFor(req->Invoke());
    EXPECT_EQ(NYT::EErrorCode::Timeout, rspOrError.GetCode());

    WaitFor(this->MyService_->GetServerStreamsAborted())
        .ThrowOnError();
}

TYPED_TEST(TNotGrpcTest, ClientNotReading)
{
    TMyProxy proxy(this->CreateChannel());
    proxy.DefaultServerAttachmentsStreamingParameters().WriteTimeout = TDuration::MilliSeconds(250);

    for (auto sleep : {false, true}) {
        auto expectedErrorCode = sleep ? NYT::EErrorCode::Timeout : NYT::EErrorCode::OK;

        auto req = proxy.StreamingEcho();
        req->set_delayed(true);
        auto invokeResult = req->Invoke();

        WaitFor(req->GetRequestAttachmentsStream()->Write(TSharedRef::FromString("hello")))
            .ThrowOnError();
        WaitFor(req->GetRequestAttachmentsStream()->Close())
            .ThrowOnError();
        WaitFor(req->GetResponseAttachmentsStream()->Read())
            .ThrowOnError();

        if (sleep) {
            Sleep(TDuration::MilliSeconds(750));
        }

        auto streamError = static_cast<TError>(
            WaitFor(req->GetResponseAttachmentsStream()->Read()));
        EXPECT_EQ(expectedErrorCode, streamError.GetCode());
        auto rspOrError = WaitFor(invokeResult);
        EXPECT_EQ(expectedErrorCode, rspOrError.GetCode());
    }
}

TYPED_TEST(TNotGrpcTest, ClientNotWriting)
{
    TMyProxy proxy(this->CreateChannel());
    proxy.DefaultServerAttachmentsStreamingParameters().ReadTimeout = TDuration::MilliSeconds(250);

    for (auto sleep : {false, true}) {
        auto expectedErrorCode = sleep ? NYT::EErrorCode::Timeout : NYT::EErrorCode::OK;

        auto req = proxy.StreamingEcho();
        auto invokeResult = req->Invoke();

        WaitFor(req->GetRequestAttachmentsStream()->Write(TSharedRef::FromString("hello")))
            .ThrowOnError();
        WaitFor(req->GetResponseAttachmentsStream()->Read())
            .ThrowOnError();

        if (sleep) {
            Sleep(TDuration::MilliSeconds(750));
        }

        auto closeError = WaitFor(req->GetRequestAttachmentsStream()->Close());
        auto readError = static_cast<TError>(
            WaitFor(req->GetResponseAttachmentsStream()->Read()));

        EXPECT_EQ(expectedErrorCode, closeError.GetCode());
        EXPECT_EQ(expectedErrorCode, readError.GetCode());
        auto rspOrError = WaitFor(invokeResult);
        EXPECT_EQ(expectedErrorCode, rspOrError.GetCode());
    }
}

TYPED_TEST(TNotGrpcTest, ServerNotReading)
{
    TMyProxy proxy(this->CreateChannel());
    proxy.DefaultClientAttachmentsStreamingParameters().WriteTimeout = TDuration::MilliSeconds(250);

    for (auto sleep : {false, true}) {
        auto expectedStreamErrorCode = sleep ? NYT::EErrorCode::Timeout : NYT::EErrorCode::OK;
        auto expectedInvokeErrorCode = sleep ? NYT::EErrorCode::Canceled : NYT::EErrorCode::OK;

        auto req = proxy.ServerNotReading();
        req->set_sleep(sleep);
        auto invokeResult = req->Invoke();

        auto data = TSharedRef::FromString("hello");
        WaitFor(req->GetRequestAttachmentsStream()->Write(data))
            .ThrowOnError();

        auto streamError = WaitFor(req->GetRequestAttachmentsStream()->Close());
        EXPECT_EQ(expectedStreamErrorCode, streamError.GetCode());
        auto rspOrError = WaitFor(invokeResult);
        EXPECT_EQ(expectedInvokeErrorCode, rspOrError.GetCode());
    }

    WaitFor(this->MyService_->GetSlowCallCanceled())
        .ThrowOnError();
}

TYPED_TEST(TNotGrpcTest, ServerNotWriting)
{
    TMyProxy proxy(this->CreateChannel());
    proxy.DefaultClientAttachmentsStreamingParameters().ReadTimeout = TDuration::MilliSeconds(250);

    for (auto sleep : {false, true}) {
        auto expectedStreamErrorCode = sleep ? NYT::EErrorCode::Timeout : NYT::EErrorCode::OK;
        auto expectedInvokeErrorCode = sleep ? NYT::EErrorCode::Canceled : NYT::EErrorCode::OK;

        auto req = proxy.ServerNotWriting();
        req->set_sleep(sleep);
        auto invokeResult = req->Invoke();

        WaitFor(req->GetResponseAttachmentsStream()->Read())
            .ThrowOnError();

        auto streamError = WaitFor(req->GetResponseAttachmentsStream()->Read());
        EXPECT_EQ(expectedStreamErrorCode, streamError.GetCode());
        auto rspOrError = WaitFor(invokeResult);
        EXPECT_EQ(expectedInvokeErrorCode, rspOrError.GetCode());
    }

    WaitFor(this->MyService_->GetSlowCallCanceled())
        .ThrowOnError();
}

TYPED_TEST(TNotGrpcTest, LaggyStreamingRequest)
{
    TMyProxy proxy(this->CreateChannel());
    proxy.DefaultServerAttachmentsStreamingParameters().ReadTimeout = TDuration::MilliSeconds(500);
    proxy.DefaultClientAttachmentsStreamingParameters().WriteTimeout = TDuration::MilliSeconds(500);

    auto req = proxy.StreamingEcho();
    req->SetRequestHeavy(true);
    req->SetResponseHeavy(true);
    req->SetSendDelay(TDuration::MilliSeconds(250));
    req->SetTimeout(TDuration::Seconds(2));
    auto invokeResult = req->Invoke();

    WaitFor(req->GetRequestAttachmentsStream()->Close())
        .ThrowOnError();
    WaitFor(ExpectEndOfStream(req->GetResponseAttachmentsStream()))
        .ThrowOnError();
    WaitFor(invokeResult)
        .ThrowOnError();
}

TYPED_TEST(TNotGrpcTest, VeryLaggyStreamingRequest)
{
    auto configText = TString(R"({
        services = {
            MyService = {
                pending_payloads_timeout = 250;
            };
        };
    })");
    auto config = ConvertTo<TServerConfigPtr>(TYsonString(configText));
    this->Server_->Configure(config);

    TMyProxy proxy(this->CreateChannel());
    proxy.DefaultServerAttachmentsStreamingParameters().ReadTimeout = TDuration::MilliSeconds(500);

    auto start = Now();

    auto req = proxy.StreamingEcho();
    req->SetRequestHeavy(true);
    req->SetResponseHeavy(true);
    req->SetSendDelay(TDuration::MilliSeconds(500));
    auto invokeResult = req->Invoke();

    auto closeError = WaitFor(req->GetRequestAttachmentsStream()->Close());
    EXPECT_EQ(NYT::EErrorCode::Timeout, closeError.GetCode());
    auto streamError = WaitFor(req->GetResponseAttachmentsStream()->Read());
    EXPECT_EQ(NYT::EErrorCode::Timeout, streamError.GetCode());
    auto rspOrError = WaitFor(invokeResult);
    EXPECT_EQ(NYT::EErrorCode::Timeout, rspOrError.GetCode());

    auto end = Now();
    int duration = (end - start).MilliSeconds();
    EXPECT_LE(duration, 2000);
}

TYPED_TEST(TNotGrpcTest, TraceBaggagePropagation)
{
    using namespace NTracing;

    auto traceContext = TTraceContext::NewRoot("Test");
    TCurrentTraceContextGuard guard(traceContext);

    auto baggage = CreateEphemeralAttributes();
    baggage->Set("key1", "value1");
    baggage->Set("key2", "value2");
    traceContext->PackBaggage(ConvertToAttributes(baggage));

    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.GetTraceBaggage();
    auto rspOrError = req->Invoke().Get();
    EXPECT_TRUE(rspOrError.IsOK());
    auto rsp = rspOrError.Value();

    auto receivedBaggage = TYsonString(rsp->baggage());
    EXPECT_EQ(receivedBaggage, ConvertToYsonString(baggage));
}

TYPED_TEST(TNotGrpcTest, DisableAcceptsBaggage)
{
    using namespace NTracing;

    auto traceContext = TTraceContext::NewRoot("Test");
    TCurrentTraceContextGuard guard(traceContext);

    auto baggage = CreateEphemeralAttributes();
    baggage->Set("key1", "value1");
    baggage->Set("key2", "value2");
    traceContext->PackBaggage(ConvertToAttributes(baggage));

    TNoBaggageProxy proxy(this->CreateChannel());
    auto req = proxy.ExpectNoBaggage();

    auto rspOrError = req->Invoke().Get();
    EXPECT_TRUE(rspOrError.IsOK());
}

TYPED_TEST(TRpcTest, ManyAsyncRequests)
{
    const int RequestCount = 1000;

    std::vector<TFuture<void>> asyncResults;

    TMyProxy proxy(this->CreateChannel());

    for (int i = 0; i < RequestCount; ++i) {
        auto request = proxy.SomeCall();
        request->set_a(i);
        auto asyncResult = request->Invoke().Apply(BIND([=] (TMyProxy::TRspSomeCallPtr rsp) {
            EXPECT_EQ(i + 100, rsp->b());
        }));
        asyncResults.push_back(asyncResult);
    }

    EXPECT_TRUE(AllSucceeded(asyncResults).Get().IsOK());
}

TYPED_TEST(TRpcTest, RegularAttachments)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.RegularAttachments();

    req->Attachments().push_back(TSharedRef::FromString("Hello"));
    req->Attachments().push_back(TSharedRef::FromString("from"));
    req->Attachments().push_back(TSharedRef::FromString("TMyProxy"));

    auto rspOrError = req->Invoke().Get();
    EXPECT_TRUE(rspOrError.IsOK());
    const auto& rsp = rspOrError.Value();

    const auto& attachments = rsp->Attachments();
    EXPECT_EQ(3u, attachments.size());
    EXPECT_EQ("Hello_",     StringFromSharedRef(attachments[0]));
    EXPECT_EQ("from_",      StringFromSharedRef(attachments[1]));
    EXPECT_EQ("TMyProxy_",  StringFromSharedRef(attachments[2]));
}

TYPED_TEST(TRpcTest, NullAndEmptyAttachments)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.NullAndEmptyAttachments();

    req->Attachments().push_back(TSharedRef());
    req->Attachments().push_back(TSharedRef::MakeEmpty());

    auto rspOrError = req->Invoke().Get();
    EXPECT_TRUE(rspOrError.IsOK());
    auto rsp = rspOrError.Value();

    const auto& attachments = rsp->Attachments();
    EXPECT_EQ(2u, attachments.size());
    EXPECT_FALSE(attachments[0]);
    EXPECT_TRUE(attachments[1]);
    EXPECT_TRUE(attachments[1].Empty());
}

TYPED_TEST(TNotGrpcTest, Compression)
{
    const auto requestCodecId = NCompression::ECodec::Zstd_2;
    const auto responseCodecId = NCompression::ECodec::Snappy;

    TString message("This is a message string.");
    std::vector<TString> attachmentStrings({
        "This is an attachment string.",
        "640K ought to be enough for anybody.",
        "According to all known laws of aviation, there is no way that a bee should be able to fly."
    });

    TMyProxy proxy(this->CreateChannel());
    proxy.SetDefaultRequestCodec(requestCodecId);
    proxy.SetDefaultResponseCodec(responseCodecId);
    proxy.SetDefaultEnableLegacyRpcCodecs(false);

    auto req = proxy.Compression();
    req->set_request_codec(static_cast<int>(requestCodecId));
    req->set_message(message);
    for (const auto& attachmentString : attachmentStrings) {
        req->Attachments().push_back(TSharedRef::FromString(attachmentString));
    }

    auto rspOrError = req->Invoke().Get();
    rspOrError.ThrowOnError();
    EXPECT_TRUE(rspOrError.IsOK());
    auto rsp = rspOrError.Value();

    EXPECT_TRUE(rsp->message() == message);
    EXPECT_TRUE(rsp->GetResponseMessage().Size() >= 2);
    const auto& serializedResponseBody = SerializeProtoToRefWithCompression(*rsp, responseCodecId);
    const auto& compressedResponseBody = rsp->GetResponseMessage()[1];
    EXPECT_TRUE(TRef::AreBitwiseEqual(compressedResponseBody, serializedResponseBody));

    const auto& attachments = rsp->Attachments();
    EXPECT_TRUE(attachments.size() == attachmentStrings.size());
    EXPECT_TRUE(rsp->GetResponseMessage().Size() == attachments.size() + 2);
    auto* responseCodec = NCompression::GetCodec(responseCodecId);
    for (int i = 0; i < std::ssize(attachments); ++i) {
        EXPECT_TRUE(StringFromSharedRef(attachments[i]) == attachmentStrings[i]);
        auto compressedAttachment = responseCodec->Compress(attachments[i]);
        EXPECT_TRUE(TRef::AreBitwiseEqual(rsp->GetResponseMessage()[i + 2], compressedAttachment));
    }
}

#if !defined(_asan_enabled_) && !defined(_msan_enabled_) && defined(_linux_)

TYPED_TEST(TRpcTest, ResponseMemoryTag)
{
    static TMemoryTag testMemoryTag = 12345;
    testMemoryTag++;
    auto initialMemoryUsage = GetMemoryUsageForTag(testMemoryTag);

    std::vector<TMyProxy::TRspPassCallPtr> rsps;
    {
        TMyProxy proxy(this->CreateChannel());
        TString longString(100, 'a');

        NYTAlloc::TMemoryTagGuard guard(testMemoryTag);

        for (int i = 0; i < 10000; ++i) {
            auto req = proxy.PassCall();
            req->SetUser(longString);
            req->SetMutationId(TGuid::Create());
            req->SetRetry(false);
            auto err = req->Invoke().Get();
            rsps.push_back(err.ValueOrThrow());
        }
    }

    auto currentMemoryUsage = GetMemoryUsageForTag(testMemoryTag);
    EXPECT_GE(currentMemoryUsage - initialMemoryUsage, 500'000u)
        << "InitialUsage: " << initialMemoryUsage << std::endl
        << "Current: " << currentMemoryUsage;
}

#endif

TYPED_TEST(TNotGrpcTest, RequestBytesThrottling)
{
    auto configText = TString(R"({
        services = {
            MyService = {
                methods = {
                    RequestBytesThrottledCall = {
                        request_bytes_throttler = {
                            limit = 1000000;
                        }
                    }
                }
            };
        };
    })");
    auto config = ConvertTo<TServerConfigPtr>(TYsonString(configText));
    this->Server_->Configure(config);

    TMyProxy proxy(this->CreateChannel());

    auto makeCall = [&] {
        auto req = proxy.RequestBytesThrottledCall();
        req->Attachments().push_back(TSharedMutableRef::Allocate(100'000));
        return req->Invoke().AsVoid();
    };

    std::vector<TFuture<void>> futures;
    for (int i = 0; i < 30; ++i) {
        futures.push_back(makeCall());
    }

    NProfiling::TWallTimer timer;
    EXPECT_TRUE(AllSucceeded(std::move(futures)).Get().IsOK());
    EXPECT_LE(std::abs(static_cast<i64>(timer.GetElapsedTime().MilliSeconds()) - 3000), 200);
}

// Now test different types of errors

TYPED_TEST(TRpcTest, OK)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.DoNothing();
    auto rspOrError = req->Invoke().Get();
    EXPECT_TRUE(rspOrError.IsOK());
}

TYPED_TEST(TRpcTest, NoAck)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.DoNothing();
    req->SetAcknowledgementTimeout(std::nullopt);
    auto rspOrError = req->Invoke().Get();
    EXPECT_TRUE(rspOrError.IsOK());
}

TYPED_TEST(TRpcTest, TransportError)
{
    TMyProxy proxy(this->CreateChannel("localhost:9999"));
    auto req = proxy.DoNothing();
    auto rspOrError = req->Invoke().Get();
    EXPECT_EQ(NRpc::EErrorCode::TransportError, rspOrError.GetCode());
}

TYPED_TEST(TRpcTest, NoService)
{
    TNonExistingServiceProxy proxy(this->CreateChannel());
    auto req = proxy.DoNothing();
    auto rspOrError = req->Invoke().Get();
    EXPECT_EQ(NRpc::EErrorCode::NoSuchService, rspOrError.GetCode());
}

TYPED_TEST(TRpcTest, NoMethod)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.NotRegistered();
    auto rspOrError = req->Invoke().Get();
    EXPECT_EQ(NRpc::EErrorCode::NoSuchMethod, rspOrError.GetCode());
}

// NB: Realms are not supported in RPC over GRPC.
TYPED_TEST(TNotGrpcTest, NoSuchRealm)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.DoNothing();
    ToProto(req->Header().mutable_realm_id(), TGuid::FromString("1-2-3-4"));
    auto rspOrError = req->Invoke().Get();
    EXPECT_EQ(NRpc::EErrorCode::NoSuchService, rspOrError.GetCode());
    EXPECT_TRUE(rspOrError.FindMatching(NRpc::EErrorCode::NoSuchRealm));
}

TYPED_TEST(TRpcTest, ClientTimeout)
{
    TMyProxy proxy(this->CreateChannel());
    proxy.SetDefaultTimeout(TDuration::Seconds(0.5));
    auto req = proxy.SlowCall();
    auto rspOrError = req->Invoke().Get();
    EXPECT_TRUE(this->CheckTimeoutCode(rspOrError.GetCode()));
}

TYPED_TEST(TRpcTest, ServerTimeout)
{
    TMyProxy proxy(this->CreateChannel());
    proxy.SetDefaultTimeout(TDuration::Seconds(0.5));
    auto req = proxy.SlowCanceledCall();
    auto rspOrError = req->Invoke().Get();
    EXPECT_TRUE(this->CheckTimeoutCode(rspOrError.GetCode()));
    WaitFor(this->MyService_->GetSlowCallCanceled())
        .ThrowOnError();
}

TYPED_TEST(TRpcTest, ClientCancel)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.SlowCanceledCall();
    auto asyncRspOrError = req->Invoke();
    Sleep(TDuration::Seconds(0.5));
    EXPECT_FALSE(asyncRspOrError.IsSet());
    asyncRspOrError.Cancel(TError("Error"));
    Sleep(TDuration::Seconds(0.1));
    EXPECT_TRUE(asyncRspOrError.IsSet());
    auto rspOrError = asyncRspOrError.Get();
    EXPECT_TRUE(this->CheckCancelCode(rspOrError.GetCode()));
    WaitFor(this->MyService_->GetSlowCallCanceled())
        .ThrowOnError();
}

TYPED_TEST(TRpcTest, SlowCall)
{
    TMyProxy proxy(this->CreateChannel());
    proxy.SetDefaultTimeout(TDuration::Seconds(2.0));
    auto req = proxy.SlowCall();
    auto rspOrError = req->Invoke().Get();
    EXPECT_TRUE(rspOrError.IsOK());
}

TYPED_TEST(TRpcTest, RequestQueueSizeLimit)
{
    TMyProxy proxy(this->CreateChannel());
    std::vector<TFuture<void>> futures;
    for (int i = 0; i < 30; ++i) {
        auto req = proxy.SlowCall();
        futures.push_back(req->Invoke().AsVoid());
    }
    Sleep(TDuration::MilliSeconds(100));
    {
        auto req = proxy.SlowCall();
        EXPECT_EQ(NRpc::EErrorCode::RequestQueueSizeLimitExceeded, req->Invoke().Get().GetCode());
    }
    EXPECT_TRUE(AllSucceeded(std::move(futures)).Get().IsOK());
}

TYPED_TEST(TRpcTest, ConcurrencyLimit)
{
    TMyProxy proxy(this->CreateChannel());
    std::vector<TFuture<void>> futures;
    for (int i = 0; i < 10; ++i) {
        auto req = proxy.SlowCall();
        futures.push_back(req->Invoke().AsVoid());
    }

    Sleep(TDuration::MilliSeconds(100));

    TFuture<void> backlogFuture;
    {
        auto req = proxy.SlowCall();
        backlogFuture = req->Invoke().AsVoid();
    }

    EXPECT_TRUE(AllSucceeded(std::move(futures)).Get().IsOK());

    Sleep(TDuration::MilliSeconds(400));
    EXPECT_FALSE(backlogFuture.IsSet());

    EXPECT_TRUE(backlogFuture.Get().IsOK());
}

TYPED_TEST(TRpcTest, NoReply)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.NoReply();
    auto rspOrError = req->Invoke().Get();
    EXPECT_EQ(NRpc::EErrorCode::Unavailable, rspOrError.GetCode());
}

TYPED_TEST(TRpcTest, CustomErrorMessage)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.CustomMessageError();
    auto rspOrError = req->Invoke().Get();
    EXPECT_EQ(NYT::EErrorCode(42), rspOrError.GetCode());
    EXPECT_EQ("Some Error", rspOrError.GetMessage());
}

TYPED_TEST(TRpcTest, ConnectionLost)
{
    TMyProxy proxy(this->CreateChannel());

    auto req = proxy.SlowCanceledCall();
    auto asyncRspOrError = req->Invoke();

    Sleep(TDuration::Seconds(0.5));

    EXPECT_FALSE(asyncRspOrError.IsSet());
    this->Server_->Stop(false);

    Sleep(TDuration::Seconds(2));

    EXPECT_TRUE(asyncRspOrError.IsSet());
    auto rspOrError = asyncRspOrError.Get();
    EXPECT_EQ(NRpc::EErrorCode::TransportError, rspOrError.GetCode());
    WaitFor(this->MyService_->GetSlowCallCanceled())
        .ThrowOnError();
}

TYPED_TEST(TNotGrpcTest, ProtocolVersionMismatch)
{
    TMyIncorrectProtocolVersionProxy proxy(this->CreateChannel());
    auto req = proxy.SomeCall();
    req->set_a(42);
    auto rspOrError = req->Invoke().Get();
    EXPECT_EQ(NRpc::EErrorCode::ProtocolError, rspOrError.GetCode());
}

TYPED_TEST(TNotGrpcTest, RequiredServerFeatureSupported)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.PassCall();
    req->RequireServerFeature(EMyFeature::Great);
    auto rspOrError = req->Invoke().Get();
    EXPECT_TRUE(rspOrError.IsOK()) << ToString(rspOrError);
}

TYPED_TEST(TNotGrpcTest, RequiredServerFeatureNotSupported)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.PassCall();
    req->RequireServerFeature(EMyFeature::Cool);
    auto rspOrError = req->Invoke().Get();
    EXPECT_EQ(NRpc::EErrorCode::UnsupportedServerFeature, rspOrError.GetCode());
    EXPECT_EQ(static_cast<int>(EMyFeature::Cool), rspOrError.Attributes().Get<int>(FeatureIdAttributeKey));
    EXPECT_EQ(ToString(EMyFeature::Cool), rspOrError.Attributes().Get<TString>(FeatureNameAttributeKey));
}

TYPED_TEST(TNotGrpcTest, RequiredClientFeatureSupported)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.RequireCoolFeature();
    req->DeclareClientFeature(EMyFeature::Cool);
    auto rspOrError = req->Invoke().Get();
    EXPECT_TRUE(rspOrError.IsOK()) << ToString(rspOrError);
}

TYPED_TEST(TNotGrpcTest, RequiredClientFeatureNotSupported)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.RequireCoolFeature();
    req->DeclareClientFeature(EMyFeature::Great);
    auto rspOrError = req->Invoke().Get();
    EXPECT_EQ(NRpc::EErrorCode::UnsupportedClientFeature, rspOrError.GetCode());
    EXPECT_EQ(static_cast<int>(EMyFeature::Cool), rspOrError.Attributes().Get<int>(FeatureIdAttributeKey));
    EXPECT_EQ(ToString(EMyFeature::Cool), rspOrError.Attributes().Get<TString>(FeatureNameAttributeKey));
}

TYPED_TEST(TRpcTest, StopWithoutActiveRequests)
{
    auto stopResult = this->MyService_->Stop();
    EXPECT_TRUE(stopResult.IsSet());
}

TYPED_TEST(TRpcTest, StopWithActiveRequests)
{
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.SlowCall();
    auto reqResult = req->Invoke();
    Sleep(TDuration::Seconds(0.5));
    auto stopResult = this->MyService_->Stop();
    EXPECT_FALSE(stopResult.IsSet());
    EXPECT_TRUE(reqResult.Get().IsOK());
    Sleep(TDuration::Seconds(0.5));
    EXPECT_TRUE(stopResult.IsSet());
}

TYPED_TEST(TRpcTest, NoMoreRequestsAfterStop)
{
    auto stopResult = this->MyService_->Stop();
    EXPECT_TRUE(stopResult.IsSet());
    TMyProxy proxy(this->CreateChannel());
    auto req = proxy.SlowCall();
    auto reqResult = req->Invoke();
    EXPECT_FALSE(reqResult.Get().IsOK());
}

////////////////////////////////////////////////////////////////////////////////

TYPED_TEST(TGrpcTest, SendMessageLimit)
{
    THashMap<TString, NYTree::INodePtr> arguments;
    arguments["grpc.max_send_message_length"] = NYT::NYTree::ConvertToNode(1);
    TMyProxy proxy(this->CreateChannel(std::nullopt, std::move(arguments)));
    auto req = proxy.SomeCall();
    req->set_a(42);
    auto error = req->Invoke().Get();
    EXPECT_EQ(NRpc::EErrorCode::ProtocolError, error.GetCode());
    EXPECT_THAT(error.GetMessage(), testing::HasSubstr("Sent message larger than max"));
}

////////////////////////////////////////////////////////////////////////////////

class TAttachmentsInputStreamTest
    : public ::testing::Test
{
protected:
    TAttachmentsInputStreamPtr CreateStream(std::optional<TDuration> timeout = {})
    {
        return New<TAttachmentsInputStream>(
            BIND([=] {}),
            nullptr,
            timeout);
    }

    static TStreamingPayload MakePayload(int sequenceNumber, std::vector<TSharedRef> attachments)
    {
        return TStreamingPayload{
            NCompression::ECodec::None,
            sequenceNumber,
            std::move(attachments)
        };
    }
};

TEST_F(TAttachmentsInputStreamTest, AbortPropagatesToRead)
{
    auto stream = CreateStream();

    auto future = stream->Read();
    EXPECT_FALSE(future.IsSet());
    stream->Abort(TError("oops"));
    EXPECT_TRUE(future.IsSet());
    EXPECT_FALSE(future.Get().IsOK());
}

TEST_F(TAttachmentsInputStreamTest, EnqueueBeforeRead)
{
    auto stream = CreateStream();

    auto payload = TSharedRef::FromString("payload");
    stream->EnqueuePayload(MakePayload(0, std::vector<TSharedRef>{payload}));

    auto future = stream->Read();
    EXPECT_TRUE(future.IsSet());
    EXPECT_TRUE(TRef::AreBitwiseEqual(payload, future.Get().ValueOrThrow()));
    EXPECT_EQ(7, stream->GetFeedback().ReadPosition);
}

TEST_F(TAttachmentsInputStreamTest, ReadBeforeEnqueue)
{
    auto stream = CreateStream();

    auto future = stream->Read();
    EXPECT_FALSE(future.IsSet());

    auto payload = TSharedRef::FromString("payload");
    stream->EnqueuePayload(MakePayload(0, std::vector<TSharedRef>{payload}));

    EXPECT_TRUE(future.IsSet());
    EXPECT_TRUE(TRef::AreBitwiseEqual(payload, future.Get().ValueOrThrow()));
    EXPECT_EQ(7, stream->GetFeedback().ReadPosition);
}

TEST_F(TAttachmentsInputStreamTest, CloseBeforeRead)
{
    auto stream = CreateStream();

    auto payload = TSharedRef::FromString("payload");
    stream->EnqueuePayload(MakePayload(0, {payload}));
    stream->EnqueuePayload(MakePayload(1, {TSharedRef()}));

    auto future1 = stream->Read();
    EXPECT_TRUE(future1.IsSet());
    EXPECT_TRUE(TRef::AreBitwiseEqual(payload, future1.Get().ValueOrThrow()));
    EXPECT_EQ(7, stream->GetFeedback().ReadPosition);

    auto future2 = stream->Read();
    EXPECT_TRUE(future2.IsSet());
    EXPECT_TRUE(!future2.Get().ValueOrThrow());
    EXPECT_EQ(8, stream->GetFeedback().ReadPosition);
}

TEST_F(TAttachmentsInputStreamTest, Reordering)
{
    auto stream = CreateStream();

    auto payload1 = TSharedRef::FromString("payload1");
    auto payload2 = TSharedRef::FromString("payload2");

    stream->EnqueuePayload(MakePayload(1, {payload2}));
    stream->EnqueuePayload(MakePayload(0, {payload1}));

    auto future1 = stream->Read();
    EXPECT_TRUE(future1.IsSet());
    EXPECT_TRUE(TRef::AreBitwiseEqual(payload1, future1.Get().ValueOrThrow()));
    EXPECT_EQ(8, stream->GetFeedback().ReadPosition);

    auto future2 = stream->Read();
    EXPECT_TRUE(future2.IsSet());
    EXPECT_TRUE(TRef::AreBitwiseEqual(payload2, future2.Get().ValueOrThrow()));
    EXPECT_EQ(16, stream->GetFeedback().ReadPosition);
}

TEST_F(TAttachmentsInputStreamTest, EmptyAttachmentReadPosition)
{
    auto stream = CreateStream();
    stream->EnqueuePayload(MakePayload(0, {TSharedMutableRef::Allocate(0)}));
    EXPECT_EQ(0, stream->GetFeedback().ReadPosition);
    auto future = stream->Read();
    EXPECT_TRUE(future.IsSet());
    EXPECT_EQ(0u, future.Get().ValueOrThrow().size());
    EXPECT_EQ(1, stream->GetFeedback().ReadPosition);
}

TEST_F(TAttachmentsInputStreamTest, Close)
{
    auto stream = CreateStream();
    stream->EnqueuePayload(MakePayload(0, {TSharedRef()}));
    auto future = stream->Read();
    EXPECT_TRUE(future.IsSet());
    EXPECT_FALSE(future.Get().ValueOrThrow());
}

TEST_F(TAttachmentsInputStreamTest, Timeout)
{
    auto stream = CreateStream(TDuration::MilliSeconds(100));
    auto future = stream->Read();
    auto error = future.Get();
    EXPECT_FALSE(error.IsOK());
    EXPECT_EQ(NYT::EErrorCode::Timeout, error.GetCode());
}

////////////////////////////////////////////////////////////////////////////////

class TAttachmentsOutputStreamTest
    : public ::testing::Test
{
protected:
    int PullCallbackCounter_;

    TAttachmentsOutputStreamPtr CreateStream(
        ssize_t windowSize,
        std::optional<TDuration> timeout = {})
    {
        PullCallbackCounter_ = 0;
        return New<TAttachmentsOutputStream>(
            NCompression::ECodec::None,
            nullptr,
            BIND([this] {
                ++PullCallbackCounter_;
            }),
            windowSize,
            timeout);
    }
};

TEST_F(TAttachmentsOutputStreamTest, NullPull)
{
    auto stream = CreateStream(100);
    EXPECT_FALSE(stream->TryPull());
}

TEST_F(TAttachmentsOutputStreamTest, SinglePull)
{
    auto stream = CreateStream(100);

    auto payload = TSharedRef::FromString("payload");
    auto future = stream->Write(payload);
    EXPECT_EQ(1, PullCallbackCounter_);
    EXPECT_TRUE(future.IsSet());
    EXPECT_TRUE(future.Get().IsOK());

    auto result = stream->TryPull();
    EXPECT_TRUE(result);
    EXPECT_EQ(0, result->SequenceNumber);
    EXPECT_EQ(1u, result->Attachments.size());
    EXPECT_TRUE(TRef::AreBitwiseEqual(payload, result->Attachments[0]));
}

TEST_F(TAttachmentsOutputStreamTest, MultiplePull)
{
    auto stream = CreateStream(100);

    std::vector<TSharedRef> payloads;
    for (int i = 0; i < 10; ++i) {
        auto payload = TSharedRef::FromString("payload" + ToString(i));
        payloads.push_back(payload);
        auto future = stream->Write(payload);
        EXPECT_EQ(i + 1, PullCallbackCounter_);
        EXPECT_TRUE(future.IsSet());
        EXPECT_TRUE(future.Get().IsOK());
    }

    auto result = stream->TryPull();
    EXPECT_TRUE(result);
    EXPECT_EQ(0, result->SequenceNumber);
    EXPECT_EQ(10u, result->Attachments.size());
    for (size_t i = 0; i < 10; ++i) {
        EXPECT_TRUE(TRef::AreBitwiseEqual(payloads[i], result->Attachments[i]));
    }
}

TEST_F(TAttachmentsOutputStreamTest, Backpressure)
{
    auto stream = CreateStream(5);

    auto payload1 = TSharedRef::FromString("abc");
    auto future1 = stream->Write(payload1);
    EXPECT_TRUE(future1.IsSet());
    EXPECT_TRUE(future1.Get().IsOK());
    EXPECT_EQ(1, PullCallbackCounter_);

    auto payload2 = TSharedRef::FromString("def");
    auto future2 = stream->Write(payload2);
    EXPECT_FALSE(future2.IsSet());
    EXPECT_EQ(2, PullCallbackCounter_);

    auto result1 = stream->TryPull();
    EXPECT_TRUE(result1);
    EXPECT_EQ(0, result1->SequenceNumber);
    EXPECT_EQ(1u, result1->Attachments.size());
    EXPECT_TRUE(TRef::AreBitwiseEqual(payload1, result1->Attachments[0]));

    EXPECT_FALSE(future2.IsSet());

    stream->HandleFeedback({3});

    EXPECT_EQ(3, PullCallbackCounter_);

    EXPECT_TRUE(future1.IsSet());
    EXPECT_TRUE(future1.Get().IsOK());

    EXPECT_TRUE(future2.IsSet());
    EXPECT_TRUE(future2.Get().IsOK());

    auto payload3 = TSharedRef::FromString("x");
    auto future3 = stream->Write(payload3);
    EXPECT_TRUE(future3.IsSet());
    EXPECT_TRUE(future3.Get().IsOK());
    EXPECT_EQ(4, PullCallbackCounter_);

    auto result2 = stream->TryPull();
    EXPECT_TRUE(result2);
    EXPECT_EQ(2u, result2->Attachments.size());
    EXPECT_TRUE(TRef::AreBitwiseEqual(payload2, result2->Attachments[0]));
    EXPECT_TRUE(TRef::AreBitwiseEqual(payload3, result2->Attachments[1]));
}

TEST_F(TAttachmentsOutputStreamTest, Abort1)
{
    auto stream = CreateStream(5);

    auto payload1 = TSharedRef::FromString("abcabc");
    auto future1 = stream->Write(payload1);
    EXPECT_FALSE(future1.IsSet());

    auto future2 = stream->Close();
    EXPECT_FALSE(future1.IsSet());

    stream->Abort(TError("oops"));

    EXPECT_TRUE(future1.IsSet());
    EXPECT_FALSE(future1.Get().IsOK());

    EXPECT_TRUE(future2.IsSet());
    EXPECT_FALSE(future2.Get().IsOK());
}

TEST_F(TAttachmentsOutputStreamTest, Abort2)
{
    auto stream = CreateStream(5);

    auto payload1 = TSharedRef::FromString("abcabc");
    auto future1 = stream->Write(payload1);
    EXPECT_FALSE(future1.IsSet());

    stream->Abort(TError("oops"));

    EXPECT_TRUE(future1.IsSet());
    EXPECT_FALSE(future1.Get().IsOK());

    auto future2 = stream->Close();
    EXPECT_TRUE(future2.IsSet());
    EXPECT_FALSE(future2.Get().IsOK());
}

TEST_F(TAttachmentsOutputStreamTest, Close1)
{
    auto stream = CreateStream(5);

    auto future = stream->Close();
    EXPECT_FALSE(future.IsSet());
    EXPECT_EQ(1, PullCallbackCounter_);

    auto result = stream->TryPull();
    EXPECT_TRUE(result);
    EXPECT_EQ(0, result->SequenceNumber);
    EXPECT_EQ(1u, result->Attachments.size());
    EXPECT_FALSE(result->Attachments[0]);

    stream->HandleFeedback({1});

    EXPECT_TRUE(future.IsSet());
    EXPECT_TRUE(future.Get().IsOK());
}

TEST_F(TAttachmentsOutputStreamTest, Close2)
{
    auto stream = CreateStream(5);

    auto payload = TSharedRef::FromString("abc");
    auto future1 = stream->Write(payload);
    EXPECT_TRUE(future1.IsSet());
    EXPECT_TRUE(future1.Get().IsOK());
    EXPECT_EQ(1, PullCallbackCounter_);

    auto future2 = stream->Close();
    EXPECT_FALSE(future2.IsSet());
    EXPECT_EQ(2, PullCallbackCounter_);

    auto result = stream->TryPull();
    EXPECT_TRUE(result);
    EXPECT_EQ(0, result->SequenceNumber);
    EXPECT_EQ(2u, result->Attachments.size());
    EXPECT_TRUE(TRef::AreBitwiseEqual(payload, result->Attachments[0]));
    EXPECT_FALSE(result->Attachments[1]);

    stream->HandleFeedback({3});

    EXPECT_FALSE(future2.IsSet());

    stream->HandleFeedback({4});

    EXPECT_TRUE(future2.IsSet());
    EXPECT_TRUE(future2.Get().IsOK());
}

TEST_F(TAttachmentsOutputStreamTest, WriteTimeout)
{
    auto stream = CreateStream(5, TDuration::MilliSeconds(100));

    auto payload = TSharedRef::FromString("abc");

    auto future1 = stream->Write(payload);
    EXPECT_TRUE(future1.IsSet());
    EXPECT_TRUE(future1.Get().IsOK());

    auto future2 = stream->Write(payload);
    EXPECT_FALSE(future2.IsSet());
    auto error = future2.Get();
    EXPECT_FALSE(error.IsOK());
    EXPECT_EQ(NYT::EErrorCode::Timeout, error.GetCode());
}

TEST_F(TAttachmentsOutputStreamTest, CloseTimeout)
{
    auto stream = CreateStream(5, TDuration::MilliSeconds(100));

    auto future = stream->Close();
    EXPECT_FALSE(future.IsSet());
    auto error = future.Get();
    EXPECT_FALSE(error.IsOK());
    EXPECT_EQ(NYT::EErrorCode::Timeout, error.GetCode());
}

TEST_F(TAttachmentsOutputStreamTest, CloseTimeout2)
{
    auto stream = CreateStream(10, TDuration::MilliSeconds(100));

    auto payload = TSharedRef::FromString("abc");

    auto future1 = stream->Write(payload);
    EXPECT_TRUE(future1.IsSet());
    EXPECT_TRUE(future1.Get().IsOK());

    auto future2 = stream->Write(payload);
    EXPECT_TRUE(future2.IsSet());
    EXPECT_TRUE(future2.Get().IsOK());

    auto future3 = stream->Close();
    EXPECT_FALSE(future3.IsSet());

    stream->HandleFeedback({3});

    EXPECT_FALSE(future3.IsSet());

    Sleep(TDuration::MilliSeconds(500));

    ASSERT_TRUE(future3.IsSet());
    auto error = future3.Get();
    EXPECT_FALSE(error.IsOK());
    EXPECT_EQ(NYT::EErrorCode::Timeout, error.GetCode());
}

////////////////////////////////////////////////////////////////////////////////

TEST(TCachingChannelFactoryTest, IdleChannels)
{
    class TChannelFactory
        : public IChannelFactory
    {
    public:
        IChannelPtr CreateChannel(const TString& /*address*/) override
        {
            return CreateLocalChannel(Server_);
        }

    private:
        const IServerPtr Server_ = CreateLocalServer();
    };

    auto factory = New<TChannelFactory>();
    auto cachingFactory = CreateCachingChannelFactory(factory, TDuration::MilliSeconds(500));
    auto channel = cachingFactory->CreateChannel("");
    EXPECT_EQ(channel, cachingFactory->CreateChannel(""));

    Sleep(TDuration::MilliSeconds(1000));
    EXPECT_EQ(channel, cachingFactory->CreateChannel(""));

    auto weakChannel = MakeWeak(channel);
    channel.Reset();

    Sleep(TDuration::MilliSeconds(1000));
    EXPECT_TRUE(weakChannel.IsExpired());
}

////////////////////////////////////////////////////////////////////////////////

} // namespace
} // namespace NYT::NRpc
