#include "inplace_traits.h"
#include "proto23_types_test.proto23.h"
#include "proto23_external_types_test.proto23.h"

static_assert(std::same_as<decltype(IntegralTypesTest::std_int32), std::int32_t>);
static_assert(std::same_as<decltype(IntegralTypesTest::proto23_int32), proto23::int32>);
static_assert(std::same_as<decltype(IntegralTypesTest::std_uint32), std::uint32_t>);
static_assert(std::same_as<decltype(IntegralTypesTest::proto23_fixed32), proto23::fixed32>);
static_assert(std::same_as<decltype(IntegralTypesTest::proto23_sfixed32), proto23::sfixed32>);
static_assert(std::same_as<decltype(IntegralTypesTest::std_int64), std::int64_t>);
static_assert(std::same_as<decltype(IntegralTypesTest::proto23_int64), proto23::int64>);
static_assert(std::same_as<decltype(IntegralTypesTest::std_uint64), std::uint64_t>);
static_assert(std::same_as<decltype(IntegralTypesTest::proto23_fixed64), proto23::fixed64>);
static_assert(std::same_as<decltype(IntegralTypesTest::plain_bool), bool>);
static_assert(std::same_as<decltype(FloatTypesTest::f), float>);
static_assert(std::same_as<decltype(FloatTypesTest::d), double>);
static_assert(std::same_as<decltype(StringTypesTest::s), std::string>);
static_assert(std::same_as<decltype(StringTypesTest::b), std::vector<std::byte>>);
static_assert(std::same_as<decltype(StringTypesTest::r), std::vector<std::vector<std::byte>>>);
static_assert(is_unique_ptr<decltype(ExtrenlTypesTest::unique_ptr_IntegralTypesTest)>::value);
static_assert(!is_unique_ptr<decltype(ExtrenlTypesTest::inplace_IntegralTypesTest)>::value);
static_assert(is_unique_ptr<decltype(ExtrenlTypesTest::unique_ptr_Duration)>::value);
static_assert(!is_unique_ptr<decltype(ExtrenlTypesTest::inplace_Duration)>::value);
static_assert(is_unique_ptr<decltype(ExtrenlTypesTest::unique_ptr_Timestamp)>::value);
static_assert(!is_unique_ptr<decltype(ExtrenlTypesTest::inplace_Timestamp)>::value);
