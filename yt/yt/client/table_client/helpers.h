#pragma once

#include "versioned_row.h"
#include "unversioned_row.h"

#include <yt/yt/core/actions/future.h>

#include <yt/yt/core/net/public.h>

#include <yt/yt/core/ytree/public.h>

namespace NYT::NTableClient {

////////////////////////////////////////////////////////////////////////////////

bool IsValidTableChunkFormat(NChunkClient::EChunkFormat chunkFormat);
bool IsTableChunkFormatVersioned(NChunkClient::EChunkFormat chunkFormat);

void ValidateTableChunkFormat(NChunkClient::EChunkFormat chunkFormat);
void ValidateTableChunkFormatAndOptimizeFor(
    NChunkClient::EChunkFormat chunkFormat,
    EOptimizeFor optimizeFor);
void ValidateTableChunkFormatVersioned(
    NChunkClient::EChunkFormat chunkFormat,
    bool versioned);

EOptimizeFor OptimizeForFromFormat(NChunkClient::EChunkFormat chunkFormat);
NChunkClient::EChunkFormat DefaultFormatFromOptimizeFor(
    EOptimizeFor optimizeFor,
    bool versioned);

////////////////////////////////////////////////////////////////////////////////

// Mostly used in unittests and for debugging purposes.
// Quite inefficient.
TUnversionedOwningRow YsonToSchemafulRow(
    const TString& yson,
    const TTableSchema& tableSchema,
    bool treatMissingAsNull,
    NYson::EYsonType ysonType = NYson::EYsonType::MapFragment);
TUnversionedOwningRow YsonToSchemalessRow(
    const TString& yson);
TVersionedRow YsonToVersionedRow(
    const TRowBufferPtr& rowBuffer,
    const TString& keyYson,
    const TString& valueYson,
    const std::vector<TTimestamp>& deleteTimestamps = {},
    const std::vector<TTimestamp>& extraWriteTimestamps = {});
TVersionedOwningRow YsonToVersionedRow(
    const TString& keyYson,
    const TString& valueYson,
    const std::vector<TTimestamp>& deleteTimestamps = {},
    const std::vector<TTimestamp>& extraWriteTimestamps = {});
TUnversionedOwningRow YsonToKey(const TString& yson);
TString KeyToYson(TUnversionedRow row);

////////////////////////////////////////////////////////////////////////////////

template <class T, class = void>
struct TUnversionedValueConversionTraits
{
    // These are conservative defaults.
    static constexpr bool Scalar = false;
    static constexpr bool Inline = false;
};

void ToUnversionedValue(TUnversionedValue* unversionedValue, std::nullopt_t, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);

void ToUnversionedValue(TUnversionedValue* unversionedValue, TGuid value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(TGuid* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, const TString& value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(TString* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, TStringBuf value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(TStringBuf* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, const char* value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(const char** value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, bool value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(bool* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, const NYson::TYsonString& value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(NYson::TYsonString* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, const NYson::TYsonStringBuf& value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(NYson::TYsonStringBuf* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, i64 value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(i64* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, ui64 value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(ui64* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, i32 value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(i32* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, ui32 value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(ui32* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, i16 value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(i16* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, ui16 value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(ui16* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, i8 value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(i8* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, ui8 value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(ui8* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, double value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(double* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, TInstant value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(TInstant* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, TDuration value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(TDuration* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, const NYTree::IMapNodePtr& value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(NYTree::IMapNodePtr* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, const NNet::TIP6Address& value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(NNet::TIP6Address* value, TUnversionedValue unversionedValue);

void ToUnversionedValue(TUnversionedValue* unversionedValue, const TError& value, const TRowBufferPtr& rowBuffer, int id = 0, EValueFlags flags = EValueFlags::None);
void FromUnversionedValue(TError* value, TUnversionedValue unversionedValue);

template <class T>
requires TEnumTraits<T>::IsEnum
void ToUnversionedValue(
    TUnversionedValue* unversionedValue,
    T value,
    const TRowBufferPtr& rowBuffer,
    int id = 0,
    EValueFlags flags = EValueFlags::None);
template <class T>
requires TEnumTraits<T>::IsEnum
void FromUnversionedValue(
    T* value,
    TUnversionedValue unversionedValue);

template <class T>
TUnversionedValue ToUnversionedValue(
    T&& value,
    const TRowBufferPtr& rowBuffer,
    int id = 0,
    EValueFlags flags = EValueFlags::None);
template <class T>
T FromUnversionedValue(TUnversionedValue unversionedValue);

template <class T>
void ToUnversionedValue(
    TUnversionedValue* unversionedValue,
    const T& value,
    const TRowBufferPtr& rowBuffer,
    int id = 0,
    EValueFlags flags = EValueFlags::None,
    typename std::enable_if<std::is_convertible<T*, ::google::protobuf::Message*>::value, void>::type* = nullptr);
template <class T>
void FromUnversionedValue(
    T* value,
    TUnversionedValue unversionedValue,
    typename std::enable_if<std::is_convertible<T*, ::google::protobuf::Message*>::value, void>::type* = nullptr);

template <class T>
void ToUnversionedValue(
    TUnversionedValue* unversionedValue,
    const std::optional<T>& value,
    const TRowBufferPtr& rowBuffer,
    int id = 0,
    EValueFlags flags = EValueFlags::None);
template <class T>
void FromUnversionedValue(
    std::optional<T>* value,
    TUnversionedValue unversionedValue);

template <class T>
void ToUnversionedValue(
    TUnversionedValue* unversionedValue,
    const std::vector<T>& values,
    const TRowBufferPtr& rowBuffer,
    int id = 0,
    EValueFlags flags = EValueFlags::None);
template <class T>
void FromUnversionedValue(
    std::vector<T>* values,
    TUnversionedValue unversionedValue,
    typename std::enable_if<std::is_convertible<T*, ::google::protobuf::Message*>::value, void>::type* = nullptr);
template <class T>
void FromUnversionedValue(
    std::vector<T>* values,
    TUnversionedValue unversionedValue,
    typename std::enable_if<TUnversionedValueConversionTraits<T>::Scalar, void>::type* = nullptr);

template <class TKey, class TValue>
void ToUnversionedValue(
    TUnversionedValue* unversionedValue,
    const THashMap<TKey, TValue>& map,
    const TRowBufferPtr& rowBuffer,
    int id = 0,
    EValueFlags flags = EValueFlags::None);
template <class TKey, class TValue>
void FromUnversionedValue(
    THashMap<TKey, TValue>* map,
    TUnversionedValue unversionedValue,
    typename std::enable_if<std::is_convertible<TValue*, ::google::protobuf::Message*>::value, void>::type* = nullptr);

//! Values get sequential ids 0..N-1 (unless wrapped into TValueWithId).
template <class... Ts>
auto ToUnversionedValues(
    const TRowBufferPtr& rowBuffer,
    Ts&&... values)
-> std::array<TUnversionedValue, sizeof...(Ts)>;

////////////////////////////////////////////////////////////////////////////////

template <class T>
void ToVersionedValue(
    TVersionedValue* versionedValue,
    T&& value,
    const TRowBufferPtr& rowBuffer,
    NTransactionClient::TTimestamp timestamp,
    int id,
    EValueFlags flags = EValueFlags::None);
template <class T>
TVersionedValue ToVersionedValue(
    T&& value,
    const TRowBufferPtr& rowBuffer,
    NTransactionClient::TTimestamp timestamp,
    int id,
    EValueFlags flags = EValueFlags::None);

////////////////////////////////////////////////////////////////////////////////

//! Unpacks individual values in #row to respective #values.
//! The number of values in #row must be greater than or equal to the number of #values.
template <class... Ts>
void FromUnversionedRow(
    TUnversionedRow row,
    Ts*... values);

//! Same as above but returns a tuple instead of placing values into the arguments.
template <class... Ts>
std::tuple<Ts...> FromUnversionedRow(TUnversionedRow row);

////////////////////////////////////////////////////////////////////////////////

//! Enables annotating values with id and flags.
template <class T>
struct TAnnotatedValue
{
    //! For an implicitly-generated deduction guide.
    TAnnotatedValue(
        const T& value,
        int id = 0,
        EValueFlags flags = EValueFlags::None)
        : Value(value)
        , Id(id)
        , Flags(flags)
    { }

    const T& Value;
    int Id;
    EValueFlags Flags;
};

//! Constructs an owning row from arbitrarily-typed values.
//! Values get sequential ids 0..N-1 (unless wrapped into TValueWithId).
template <class... Ts>
TUnversionedOwningRow MakeUnversionedOwningRow(Ts&&... values);

////////////////////////////////////////////////////////////////////////////////

class TUnversionedRowsBuilder
    : private TNonCopyable
{
public:
    TUnversionedRowsBuilder();
    explicit TUnversionedRowsBuilder(TRowBufferPtr rowBuffer);

    void ReserveRows(int rowCount);

    void AddRow(TUnversionedRow row);
    void AddRow(TMutableUnversionedRow row);
    void AddProtoRow(const TString& protoRow);

    //! Values get sequential ids 0..N-1 (unless wrapped into TValueWithId).
    template <class... Ts>
    void AddRow(Ts&&... values);

    TSharedRange<TUnversionedRow> Build();

private:
    const TRowBufferPtr RowBuffer_;

    std::vector<TUnversionedRow> Rows_;
};

////////////////////////////////////////////////////////////////////////////////

//! A helper encapsulating Read/GetReadyEvent calls for a typical row batch reader.
/*!
 *  Invokes |Read| method and checks the result for emptiness.
 *  If empty, waits for the ready event and loops.
 *  Returns either a non-empty batch or null (indicating end-of-stream).
 *
 *  All additional parameters are forwarded to |Read| call.
 */
template <class TReader, class... TArgs>
auto ReadRowBatch(const TIntrusivePtr<TReader>& reader, TArgs&&... args);

//! A helper encapsulating Write/GetReadyEvent calls for a typical row batch writer.
/*!
 *  Invokes |Write| method and checks the result.
 *  If false, waits for the ready event.
 *
 *  All additional parameters are forwarded to |Write| call.
 */
template <class TWriter, class... TArgs>
auto WriteRowBatch(const TIntrusivePtr<TWriter>& writer, TArgs&&... args);

////////////////////////////////////////////////////////////////////////////////

void UnversionedValueToYson(TUnversionedValue unversionedValue, NYson::IYsonConsumer* consumer);
void UnversionedValueToYson(TUnversionedValue unversionedValue, NYson::TCheckedInDebugYsonTokenWriter* tokenWriter);
NYson::TYsonString UnversionedValueToYson(TUnversionedValue unversionedValue, bool enableRaw = false);

////////////////////////////////////////////////////////////////////////////////

void ToAny(TRowBuffer* context, TUnversionedValue* result, TUnversionedValue* value, NYson::EYsonFormat format = NYson::EYsonFormat::Binary);

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NTableClient

#define HELPERS_INL_H_
#include "helpers-inl.h"
#undef HELPERS_INL_H_
