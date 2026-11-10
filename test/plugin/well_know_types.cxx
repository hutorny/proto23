#include <google/protobuf/any.proto23.h>
#include <google/protobuf/api.proto23.h>
#include <google/protobuf/duration.proto23.h>
#include <google/protobuf/empty.proto23.h>
#include <google/protobuf/field_mask.proto23.h>
#include <google/protobuf/source_context.proto23.h>
#include <google/protobuf/timestamp.proto23.h>
#include <google/protobuf/wrappers.proto23.h>

#include "harness.h"

namespace {
using namespace google::protobuf;
[[maybe_unused]] void compile_roundtrip(std::istream& in, std::ostream& out) {
    testing::harness::all<Any, Api, Duration, Empty, FieldMask, SourceContext, Timestamp>(in, out);
    testing::harness::all<DoubleValue, FloatValue, Int64Value, UInt64Value, Int32Value, UInt32Value,
                          BoolValue, StringValue, BytesValue>(in, out);
}
} // namespace
