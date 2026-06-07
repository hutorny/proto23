#include <proto23_inplace_test.proto23.h>

template<typename T> struct is_unique_ptr : std::false_type {};
template<typename T> struct is_unique_ptr<std::unique_ptr<T>> : std::true_type {};
template<typename T> struct is_optional : std::false_type {};
template<typename T> struct is_optional<std::optional<T>> : std::true_type {};
template<typename T> struct is_inplace_vector : std::false_type {};
template<typename T, std::size_t N> struct is_inplace_vector<proto23::inplace_vector<T, N>> : std::true_type {};
template<typename T> struct is_inplace_string : std::false_type {};
template<std::size_t N> struct is_inplace_string<proto23::inplace_string<N>> : std::true_type {};


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

