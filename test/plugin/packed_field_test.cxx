#include "packed_field_test.proto23.h"
#include "harness.h"

namespace {
using namespace google::protobuf::python::internal;
[[maybe_unused]] void compile_roundtrip(std::istream& in, std::ostream& out) {
    testing::harness::all<TestPackedTypes, TestUnpackedTypes>(in, out);
}
} // namespace
