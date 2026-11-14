#include "unittest_well_known_types.proto23.h"
#include "harness.h"

namespace {
[[maybe_unused]] void compile_roundtrip(std::istream& in, std::ostream& out) {
    testing::harness::all<TestWellKnownTypes, RepeatedWellKnownTypes, OneofWellKnownTypes, MapWellKnownTypes>(in, out);
}
}
