#include <type_traits>
#include <optional>
#include <proto23/options.proto23.h>

template<typename T> struct is_unique_ptr : std::false_type {};
template<typename T> struct is_unique_ptr<std::unique_ptr<T>> : std::true_type {};
template<typename T> struct is_optional : std::false_type {};
template<typename T> struct is_optional<std::optional<T>> : std::true_type {};
template<typename T> struct is_inplace_vector : std::false_type {};
template<typename T, std::size_t N> struct is_inplace_vector<proto23::inplace_vector<T, N>> : std::true_type {};
template<typename T> struct is_inplace_string : std::false_type {};
template<std::size_t N> struct is_inplace_string<proto23::inplace_string<N>> : std::true_type {};
