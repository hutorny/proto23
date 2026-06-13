#include "inplace_traits.h"
#include <proto23_inplace_file_test.proto23.h>
#include <proto23_inplace_test.proto23.h>

static_assert(is_unique_ptr<std::unique_ptr<long>>::value);
static_assert(is_unique_ptr<decltype(InplaceByDefaultTest::unique_ptr_OuterPDO)>::value);
static_assert(std::same_as<decltype(Outer::int32), std::int32_t>);
static_assert(!is_unique_ptr<decltype(InplaceByDefaultTest::inplace_inner)>::value);
static_assert(!is_optional<decltype(InplaceByDefaultTest::inplace_inner)>::value);
static_assert(is_unique_ptr<decltype(InplaceByDefaultTest::unique_ptr_NotComplete)>::value);
static_assert(is_optional<decltype(InplaceByDefaultTest::optional_Inner)>::value);
static_assert(is_unique_ptr<decltype(InplaceByDefaultTest::unique_ptr_NotComplete2)>::value);

static_assert(!is_optional<decltype(ExplicitInplace::inplace_Outer)>::value);
static_assert(!is_unique_ptr<decltype(ExplicitInplace::inplace_Outer)>::value);
static_assert(!is_optional<decltype(ExplicitInplace::inplace_Inner)>::value);
static_assert(!is_unique_ptr<decltype(ExplicitInplace::inplace_Inner)>::value);
static_assert(is_unique_ptr<decltype(ExplicitInplace::unique_ptr_NotComplete)>::value);
static_assert(is_optional<decltype(ExplicitInplace::optional_Inner)>::value);
static_assert(is_unique_ptr<decltype(ExplicitInplace::unique_ptr_NotComplete2)>::value);
static_assert(is_optional<decltype(ExplicitInplace::optional_and_)>::value);
static_assert(!is_optional<decltype(ExplicitInplace::inplace_and_)>::value);
static_assert(!is_unique_ptr<decltype(ExplicitInplace::inplace_and_)>::value);
static_assert(is_inplace_vector<decltype(ExplicitInplace::inplace_vector_Inner)>::value);
static_assert(is_inplace_string<decltype(ExplicitInplace::inplace_string_200)>::value);
static_assert(!is_inplace_vector<decltype(ExplicitInplace::std_vector_NotComplete)>::value);

static_assert(!is_unique_ptr<decltype(InplaceByFileDefaultTest::inplace_Outer)>::value);
static_assert(!is_unique_ptr<decltype(InplaceByFileDefaultTest::inplace_Timestamp)>::value);
static_assert(!is_unique_ptr<decltype(InplaceByFileDefaultTest::inplace_inner)>::value);
static_assert(is_unique_ptr<decltype(InplaceByFileDefaultTest::unique_ptr_NotComplete)>::value);
static_assert(is_optional<decltype(InplaceByFileDefaultTest::optional_Inner)>::value);
static_assert(is_unique_ptr<decltype(InplaceByFileDefaultTest::unique_ptr_NotComplete2)>::value);
static_assert(is_optional<decltype(InplaceByFileDefaultTest::optional_Outer)>::value);
static_assert(is_inplace_string<decltype(InplaceByFileDefaultTest::inplace_string)>::value);
static_assert(is_inplace_vector<decltype(InplaceByFileDefaultTest::inplace_vector)>::value);

static_assert(is_unique_ptr<decltype(ExplicitNotInplace::unique_ptr_Outer)>::value);
static_assert(is_unique_ptr<decltype(ExplicitNotInplace::unique_ptr_Timestamp)>::value);
static_assert(is_unique_ptr<decltype(ExplicitNotInplace::unique_ptr_Inner)>::value);
static_assert(is_unique_ptr<decltype(ExplicitNotInplace::unique_ptr_NotComplete2)>::value);
static_assert(is_unique_ptr<decltype(ExplicitNotInplace::unique_ptr_Inner2)>::value);
static_assert(is_unique_ptr<decltype(ExplicitNotInplace::unique_ptr_NotComplete)>::value);
static_assert(!is_inplace_vector<decltype(ExplicitNotInplace::vector_Inner)>::value);
static_assert(!is_inplace_string<decltype(ExplicitNotInplace::std_string_200)>::value);
static_assert(!is_inplace_vector<decltype(ExplicitNotInplace::std_vector_NotComplete)>::value);

