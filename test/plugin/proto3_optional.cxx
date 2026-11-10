#include "proto3_optional.proto23.h"
#include "harness.h"

namespace {
[[maybe_unused]] void compile_roundtrip(std::istream& in, std::ostream& out) {
    testing::harness::all<Proto3Optional>(in, out);
}
} // namespace
