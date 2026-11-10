#include "proto23_message_id_test.proto23.h"
#include "harness.h"

namespace {
[[maybe_unused]] void compile_roundtrip(std::istream& in, std::ostream& out) {
    testing::harness::all<test::MessageIDTest>(in, out);
}
} // namespace
