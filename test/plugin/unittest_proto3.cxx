#include "unittest_proto3.proto23.h"
#include <proto23/options.proto23.h>
static_assert(std::is_trivially_copyable_v<proto23::inplace_string<64>>);
static_assert(std::is_trivially_copyable_v<proto23::inplace_vector<long, 16>>);
static_assert(proto23::enum_traits<ForeignEnum>::min() == ForeignEnum::FOREIGN_ZERO);
static_assert(proto23::enum_traits<ForeignEnum>::max() == ForeignEnum::FOREIGN_LARGE);
static_assert(proto23::enum_traits<ForeignEnum>::count() == 5);
static_assert(!proto23::enum_traits<ForeignEnum>::is_sequence);
static_assert(proto23::enum_traits<TestOneof2::NestedEnum>::min() == TestOneof2::NestedEnum::UNKNOWN);
static_assert(proto23::enum_traits<TestOneof2::NestedEnum>::max() == TestOneof2::NestedEnum::BAZ);
static_assert(proto23::enum_traits<TestOneof2::NestedEnum>::mask() == 3);
static_assert(proto23::enum_traits<TestOneof2::NestedEnum>::count() == 4);
static_assert(proto23::enum_traits<TestOneof2::NestedEnum>::is_sequence);
