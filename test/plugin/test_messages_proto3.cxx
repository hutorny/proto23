#include "test_messages_proto3.proto23.h"
#include "harness.h"

namespace {
[[maybe_unused]] void compile_roundtrip(std::istream& in, std::ostream& out) {
    using namespace protobuf_test_messages::proto3;
    testing::harness::all<ForeignMessage, TestAllTypesProto3>(in, out);
}
} // namespace
