#include "unittest_runtime_proto3.proto23.h"
#include "harness.h"

namespace {
[[maybe_unused]] void compile_roundtrip(std::istream& in, std::ostream& out) {
    testing::harness::all<Message3, Message3Optional>(in, out);
}
} // namespace
