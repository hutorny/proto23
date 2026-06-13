/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * proto/proto.h - C++23 Protobuf serializer/deserializer with explicit field listing
 *
 * Backport of proto26 (C++26 reflection-based) to C++23.
 * Instead of [[=N]] reflection annotations, each serializable struct declares:
 *   using Model = proto::Fields<proto::Field<&Foo::x, 1>, proto::Field<&Foo::y, 2>>;
 *
 * Wire format and semantics are identical to the C++26 implementation.
 *
 * Licensed under MIT License, see full text in LICENSE
 * or visit page https://opensource.org/license/mit/
 */
#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <proto23/inplace_buffer.h>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace proto23 {

// =============================================================================
// Protobuf wire types — enum types to distinguish how field should be encoded
// =============================================================================

/// Protobuf int32: varint, two's-complement negative (10 bytes worst case)
enum int32   : std::int32_t  {};
/// Protobuf sint32: zigzag-encoded varint
enum sint32  : std::int32_t  {};
/// Protobuf uint32: unsigned varint
enum uint32  : std::uint32_t {};
/// Protobuf fixed32: little-endian 32-bit unsigned
enum fixed32 : std::uint32_t {};
/// Protobuf sfixed32: little-endian 32-bit signed
enum sfixed32 : std::int32_t {};
/// Protobuf int64: plain varint
enum int64   : std::int64_t  {};
/// Protobuf sint64: zigzag-encoded varint
enum sint64  : std::int64_t  {};
/// Protobuf uint64: unsigned varint
enum uint64  : std::uint64_t {};
/// Protobuf fixed64: little-endian 64-bit unsigned
enum fixed64 : std::uint64_t {};
/// Protobuf sfixed64: little-endian 64-bit signed
enum sfixed64 : std::int64_t {};

/// Packing mode for repeated scalar fields
enum class packed_t : std::uint8_t {
    default_,   ///< Proto3: packed for scalars, unpacked for messages/strings
    yes,        ///< Always packed
    no          ///< Always unpacked (one tag per element)
};

/// Errors, occurred during deserialization (a set of declared bits)
enum class parse_error : std::uint8_t {
  none = 0,
  type_mismatch = 1,
  integer_overflow = 2,
  array_overrun = 4,
  string_overrun = 8,
  invalid_length = 16,
  tellg_fail = 32,
};

// =============================================================================
// Internal concepts and wire-format helpers
// =============================================================================
namespace detail {

/// True if type A equals any of types B...
template<template<typename, typename> class Pred, typename A, typename... B>
consteval bool anyof() { return std::disjunction_v<Pred<A, B>...>; }

template<typename T>
concept numeric_enum = anyof<std::is_same, T,
    proto23::int32, proto23::sint32, proto23::uint32, proto23::fixed32, proto23::sfixed32,
    proto23::int64, proto23::sint64, proto23::uint64, proto23::fixed64, proto23::sfixed64>();

/// User-defined enum (not one of the proto wire-type enums above)
template<typename T>
concept enumeration = std::is_enum_v<T> && !numeric_enum<T>;

/// Varint-encoded types (wire type 0, no zigzag)
template<typename T>
concept variant_integral =
    anyof<std::is_same, T,
        bool, std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t,
        proto23::int32, proto23::uint32, proto23::int64, proto23::uint64>()
    || enumeration<T>;

/// Zigzag-encoded types (wire type 0, zigzag)
template<typename T>
concept zigzag_integral =
    anyof<std::is_same, T,
        std::int8_t, std::int16_t, std::int32_t, std::int64_t,
        proto23::sint32, proto23::sint64>();

/// Fixed-width binary types (wire type 1 or 5)
template<typename T>
concept fixed_arithmetic =
    anyof<std::is_same, T,
        proto23::fixed32, proto23::sfixed32,
        proto23::fixed64, proto23::sfixed64,
        float, double>();

/// Any numeric proto type
template<typename T>
concept arithmetic = variant_integral<T> || zigzag_integral<T> || fixed_arithmetic<T>;

/// Scalar = numeric or user enum
template<typename T>
concept scalar = enumeration<T> || arithmetic<T>;

/// Byte-like element stored as raw bits (fixed_arithmetic or char/byte variants)
template<typename T>
concept fixed_scalar = fixed_arithmetic<T>
    || anyof<std::is_same, T, char, signed char, unsigned char, std::byte>();

// --- Container traits ---

template<typename C>
concept back_insertable = requires(C c, typename C::value_type&& v) {
    c.emplace_back(std::move(v));
};

template<typename C>
concept resizable = requires(C c) { c.resize(std::size_t{1U}); };

template<typename E>
concept elementary = std::default_initializable<E>
    && requires(E& dst, E src) { dst = std::move(src); };

/// Fixed-size array or non-resizable output range of elementary elements
template<typename A>
concept bounded_array =
    (std::is_bounded_array_v<A> && elementary<std::remove_extent_t<A>>)
    || (std::ranges::output_range<A, typename A::value_type>
        && !resizable<A> && elementary<typename A::value_type>);

/// Resizable container with emplace_back
template<typename C>
concept container = back_insertable<C> && elementary<typename C::value_type> && requires(C& c) {
  c.size();
  c.max_size();
};

/// Map-like container
template<typename C>
concept associative = requires(C c,
        typename C::key_type k, typename C::mapped_type v) {
    c.emplace(std::make_pair(k, v));
};

/// Fixed-element array: bounded_array whose elements are raw binary (fixed_scalar)
template<typename A>
concept fixed_array = bounded_array<A>
    && (fixed_scalar<std::remove_extent_t<A>>          // C-style: T[N]
        || fixed_scalar<typename A::value_type>);      // std::array<T,N>

/// Contiguous resizable array of raw binary elements (e.g. vector<byte>, vector<float>)
template<typename A>
concept contiguous_resizable_array =
    container<A>
    && std::ranges::contiguous_range<A>
    && resizable<A>
    && fixed_scalar<typename A::value_type>
    && requires(A& a) { a.data(); };

/// Packable type - container of scalars
template<typename A>
concept packable = container<A> && scalar<typename A::value_type>;

//
template<typename T>
concept string_like = std::is_convertible_v<T, std::string_view> && requires(T& s) {
  s.resize(1Z);
};

// --- Wire format type IDs (matches protobuf spec) ---

enum class data_type : std::uint8_t {
    variant = 0U,  ///< Varint
    fixed64 = 1U,  ///< 64-bit little-endian
    lengthy = 2U,  ///< Length-delimited
    fixed32 = 5U,  ///< 32-bit little-endian
};

// matches<T>: returns true when wire type is compatible with C++ type T

template<variant_integral T>
[[nodiscard]] constexpr bool matches(data_type t) noexcept {
    return t == data_type::variant;
}
template<zigzag_integral T>
[[nodiscard]] constexpr bool matches(data_type t) noexcept {
    return t == data_type::variant;
}
template<fixed_arithmetic T>
[[nodiscard]] constexpr bool matches(data_type t) noexcept {
    return (t == data_type::fixed32 && sizeof(T) == sizeof(proto23::fixed32))
        || (t == data_type::fixed64 && sizeof(T) == sizeof(proto23::fixed64));
}
template<typename T>
[[nodiscard]] constexpr bool matches(data_type t) noexcept {
    return t == data_type::lengthy; // FIXME match unpacked containers
}

//[[nodiscard]] constexpr bool is_unpacked_for(packed_t p, bool is_scalar_element) noexcept {
//    return (p == packed_t::no)
//        || (p == packed_t::default_ && !is_scalar_element);
//}

template<packed_t Pack, container Container>
constexpr bool unpacked = (Pack == packed_t::no)
  || (Pack == packed_t::default_ && ! scalar<typename Container::value_type>);

template<typename T, bool=std::is_enum_v<T>>
struct ul_type;

template<typename T>
struct ul_type<T, false> {
  using type = T;
};

template<typename T>
struct ul_type<T, true> {
  using type = std::underlying_type_t<T>;
};

template<typename T>
parse_error saturating_assign(T& variable, std::uint64_t value) noexcept {
    using ul_t = typename ul_type<T>::type;
    using val_t = std::conditional_t<std::is_signed_v<ul_t>, std::int64_t, std::uint64_t>;
    constexpr auto max = static_cast<val_t>(std::numeric_limits<ul_t>::max());
    constexpr auto min = static_cast<val_t>(std::numeric_limits<ul_t>::min());
    if (static_cast<val_t>(value) > max) {
      variable = static_cast<T>(max);
      return parse_error::integer_overflow;
    }
    if constexpr(std::is_signed_v<ul_t>) {
      if (static_cast<val_t>(value) < min) {
        variable = static_cast<T>(min);
        return parse_error::integer_overflow;
      }
    }
    variable = static_cast<T>(value);
    return parse_error::none;
}

} // namespace detail

// =============================================================================
// Member-pointer traits
//      optional<T> and unique_ptr<T> are "unwrapped" so that the wire-type
//      match is performed on the inner type T, not the wrapper.
// =============================================================================

template<typename>
struct member_traits;

template<class C, typename T>
struct member_traits<T C::*> {
    using class_type = C;
    using value_type = T;
};

template<class C, typename T>
struct member_traits<std::optional<T> C::*> {
    using class_type = C;
    using value_type = T;            ///< unwrapped: used for wire-type matching
};

template<class C, typename T>
struct member_traits<std::unique_ptr<T> C::*> {
    using class_type = C;
    using value_type = T;            ///< unwrapped: used for wire-type matching
};

template<typename Message>
struct external_model;

namespace detail {

template<typename Message>
concept has_external_model = requires {
  typename external_model<Message>::type;
};

template<typename Message>
concept has_model = requires {
  typename std::remove_cvref_t<Message>::Model;
};

template<class Message, bool = has_model<Message>, bool = has_external_model<Message>>
struct model_lookup;

template<class Message>
struct model_lookup<Message, true, false> {
  using type = typename Message::Model;
};

template<class Message>
struct model_lookup<Message, false, true> {
  using type = typename external_model<Message>::type;
};

template<class Message>
struct model_lookup<Message, true, true> {
  // External model wins
  using type = typename external_model<Message>::type;
};
} //namespace detail

template<class Message>
using model = typename detail::model_lookup<Message>::type;

// =============================================================================
// Field descriptors
// =============================================================================

/// Regular field: pointer-to-member + field number + optional packing override
template<auto Member, int Num, packed_t Pack = packed_t::default_>
struct Field {
    static constexpr auto    member   = Member;
    static constexpr int     num      = Num;
    static constexpr packed_t packing = Pack;
    using member_ptr_type = decltype(Member);
    using class_type      = typename member_traits<member_ptr_type>::class_type;
    /// Wire-type matching uses the unwrapped type (inner T for optional/unique_ptr)
    using value_type      = typename member_traits<member_ptr_type>::value_type;
    static_assert(Pack == packed_t::default_ || detail::packable<value_type>,
        "Pack can only be specified for container of primitive values.");
    static_assert(!(detail::bounded_array<value_type> && Pack == packed_t::no),
        "Unpacked bounded arrays are not supported, suggested vector instead");
};

/// Packed repeated field (forced packed encoding)
template<auto Member, int Num>
using PackedField = Field<Member, Num, packed_t::yes>;

/// Unpacked repeated field (one tag per element)
template<auto Member, int Num>
using UnpackedField = Field<Member, Num, packed_t::no>;

/// Oneof / variant field: each of Nums... maps to the variant alternative at that position.
/// E.g. OneOf<&Foo::oneof, 4, 5, 6> means alternative 0 ↔ field 4,
///                                        alternative 1 ↔ field 5, etc.
template<auto Member, int... Nums>
struct OneOf {
    static constexpr auto   member    = Member;
    static constexpr std::size_t num_count = sizeof...(Nums);
    static constexpr std::array<int, sizeof...(Nums)> nums = {Nums...};

    using member_ptr_type = decltype(Member);
    using class_type      = typename member_traits<member_ptr_type>::class_type;
    using value_type      = typename member_traits<member_ptr_type>::value_type;
};

// =============================================================================
// Fields<...> — the Model type that a struct nests as `using Model = ...`
// =============================================================================

namespace detail {
template<typename T> struct is_oneof_desc      : std::false_type {};
template<auto M, int... Ns>
struct is_oneof_desc<OneOf<M, Ns...>>           : std::true_type  {};

template<typename T> struct is_fields_type     : std::false_type {};
} // namespace detail

/// Collection of field descriptors forming the wire model of a struct.
///
/// Example:
/// @code
///   struct Person {
///     std::string name;
///     std::int32_t age{};
///     using Model = proto::Fields<
///         proto::Field<&Person::name, 1>,
///         proto::Field<&Person::age,  2>>;
///   };
/// @endcode
template<typename... FieldDescs>
requires (sizeof...(FieldDescs) !=0 )
struct Fields {
    using fields_tuple = std::tuple<FieldDescs...>;
    static constexpr std::size_t field_count = sizeof...(FieldDescs);
};

namespace detail {
template<typename... Fs>
struct is_fields_type<Fields<Fs...>> : std::true_type {};

template<auto Field, typename Model, std::size_t I>
constexpr std::size_t visit_field() {
  using field_type = std::tuple_element_t<I, typename Model::fields_tuple>;
  if constexpr (std::equality_comparable_with<decltype(Field), typename field_type::member_ptr_type>) {
    if(Field == field_type::member) return I;
  }
  if constexpr(I + 1 == Model::field_count) {
    return I + 1;
  } else {
    return visit_field<Field, Model, I+1>();
  }
}


template<auto Field, typename Model>
constexpr std::size_t find_field() noexcept {
  return visit_field<Field, Model, 0Z>();
}

} // namespace detail

inline constexpr auto operator|(parse_error a, parse_error b) {
  return static_cast<parse_error>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

inline constexpr auto operator&(parse_error a, parse_error b) {
  return static_cast<parse_error>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

inline parse_error& operator|=(parse_error& a, parse_error b) {
  a = static_cast<parse_error>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
  return a;
}

namespace detail {
// =============================================================================
// Result<N> — tracks which fields were (de)serialized
// =============================================================================

/// Deserialization result: one bit per field descriptor in the model.
/// Zero means the field was absent;
template<std::size_t N>
class result_type {
public:
    void set(std::size_t i) noexcept {
        flags[i] = 1;
    }
    void set(std::size_t i, parse_error err) noexcept {
        flags[i] = 1;
        errors_ |= err;
    }
    void set(parse_error err) noexcept {
        errors_ |= err;
    }
    constexpr auto errors() const noexcept {
      return errors_;
    }
    template<std::size_t I>
    [[nodiscard]] bool test() const noexcept {
      static_assert(I < N, "Filed index out of bounds");
      return flags.test(I);
    }
    constexpr auto any() const noexcept { return flags.any(); }
    constexpr auto all() const noexcept { return flags.all(); }
private:
    std::bitset<N> flags{};
    parse_error errors_{};
};

/// any() for a plain count
//[[nodiscard]] inline constexpr bool any(std::size_t count) noexcept { return count != 0U; }

/// Map Fields<Fs...> -> Result<N>
template<typename Model>
struct result_for;
template<typename... Fs>
struct result_for<Fields<Fs...>> { using type = result_type<sizeof...(Fs)>; };
}
// =============================================================================
// message concept + convenience alias
// =============================================================================

template<typename Message>
concept message = std::is_class_v<std::remove_cvref_t<Message>>
    && (detail::has_model<Message> || detail::has_external_model<Message>)
    && detail::is_fields_type<model<Message>>::value;

template<message Message>
class deserialize_result : private detail::result_for<model<Message>>::type {
public:
  using base = typename detail::result_for<model<Message>>::type;
  using base::errors;
  using base::all;
  using base::any;
  using base::set;
  constexpr deserialize_result() = default;
  constexpr explicit deserialize_result(parse_error err) { set(err); }
  template<auto Field>
  constexpr bool has() const noexcept {
    static constexpr std::size_t I = detail::find_field<Field, model<Message>>();
    static_assert(I < model<Message>::field_count, "Filed not found in the model");
    return base::template test<I>();
  }
};

template<message  Message>
[[nodiscard]] constexpr bool all(const deserialize_result<Message>& r) noexcept {
    return r.all();
}

template<message  Message>
[[nodiscard]] constexpr bool any(const deserialize_result<Message>& r) noexcept {
    return r.any();
}

// =============================================================================
// Map-entry helper
// =============================================================================
namespace detail {

template<typename Key, typename Value>
struct MapEntry {
    Key   key{};
    Value value{};
    using Model = Fields<Field<&MapEntry::key,   1>,
                         Field<&MapEntry::value, 2>>;
};

} // namespace detail

// =============================================================================
// Wire-format I/O utilities
// =============================================================================
namespace detail {

enum class quantity : std::uint8_t {
  none, some
};

struct field_result {
  quantity read;
  parse_error errors;
};

struct try_field_result : field_result {
  std::uint16_t id;
  static constexpr auto fail = std::numeric_limits<std::uint16_t>::max();
};


using length_type = std::size_t;
inline constexpr length_type max_length   = std::numeric_limits<length_type>::max();
inline constexpr std::size_t one_element  = 1U;
inline constexpr std::size_t none         = 0U;
inline constexpr std::size_t max_valid_length = 0x80000000Z;
inline constexpr std::size_t max_valid_string_length = 64Z*1024Z*1024Z;

// ---- Reading ----

[[nodiscard]] inline std::uint64_t read_varint(std::istream& in) {
    static constexpr std::uint8_t high_bit = 0x80U;
    std::uint64_t val{};
    char chr{};
    unsigned shift = 0U;
    for (bool done = false;
         !done && in.get(chr).good();
         done = (static_cast<std::uint8_t>(chr) & high_bit) == 0U, shift += 7U)
    {
        val |= static_cast<std::uint64_t>(
                   static_cast<std::uint8_t>(chr) & 0x7FU) << shift;
    }
    return val;
}

[[nodiscard]] inline auto read_id_type(std::istream& in) {
    const auto encoded = read_varint(in);
    return std::make_tuple(
        static_cast<int>(encoded >> 3U),
        static_cast<data_type>(static_cast<std::uint8_t>(encoded & 7U)));
}

[[nodiscard]] inline length_type read_length(std::istream& in) {
    auto result = static_cast<length_type>(read_varint(in));
    if (result > max_valid_length) {
      result = 0;
      in.bad();
    }
    return result;
}

[[nodiscard]] inline length_type get_pos(std::istream& in) {
    return static_cast<length_type>(in.tellg());
}

[[nodiscard]] inline length_type get_eod(std::istream& in) {
    const auto len = read_length(in);
    return len + get_pos(in);
}

[[nodiscard]] inline length_type unlimited([[maybe_unused]] std::istream&) noexcept {
    return max_length;
}

inline void skip(std::istream& in, data_type t) {
    switch (t) {
    case data_type::fixed32:
        in.ignore(static_cast<std::streamsize>(sizeof(proto23::fixed32)));
        break;
    case data_type::fixed64:
        in.ignore(static_cast<std::streamsize>(sizeof(proto23::fixed64)));
        break;
    case data_type::variant:
        (void)read_varint(in);
        break;
    case data_type::lengthy:
        in.ignore(static_cast<std::streamsize>(read_varint(in)));
        break;
    default:
        break;
    }
}

// ---- Writing ----

inline void write_varint(std::ostream& out, std::uint64_t v) {
    do {
        const auto byte = static_cast<char>(
            (v & 0x7FU) | (v > 0x7FU ? 0x80U : 0U));
        out.put(byte);
        v >>= 7U;
    } while (v > 0U);
}

inline void write_id_type(std::ostream& out, int num, data_type wt) {
    write_varint(out,
        (static_cast<std::uint64_t>(num) << 3U) | static_cast<std::uint64_t>(wt));
}

inline void write_length(std::ostream& out, length_type len) {
    write_varint(out, static_cast<std::uint64_t>(len));
}

// ---- Zigzag ----

template<typename T>
[[nodiscard]] constexpr std::uint64_t encode_zigzag(T v) noexcept {
    using U = std::conditional_t<sizeof(T) == 8, std::uint64_t, std::uint32_t>;
    return static_cast<std::uint64_t>((static_cast<U>(v) << 1U) ^ (v < 0 ? ~U{0} : U{0}));
}

[[nodiscard]] inline constexpr std::uint64_t decode_zigzag(std::uint64_t v) noexcept {
    return (v >> 1U) ^ (-(v & 1U));
}

} // namespace detail

// =============================================================================
// Forward declarations (mutual recursion between message deserializer
//        and the nested-message overload of detail::deserialize)
// =============================================================================
template<message Message>
[[nodiscard]] deserialize_result<Message>
deserialize(std::istream& in, Message& msg);

template<message Message>
void serialize(std::ostream& out, const Message& msg);

namespace detail {

template<proto23::message Message>
[[nodiscard]] deserialize_result<Message>
deserialize_message(std::istream& in, Message& msg,
                    length_type (*eod_fn)(std::istream&));

/// Forward declaration so optional<T>/unique_ptr<T> overloads can call it.
template<proto23::message Message>
[[nodiscard]] field_result deserialize(std::istream& in, Message& msg);
template<class Message>
requires (std::is_empty_v<Message>)
[[nodiscard]] field_result deserialize(std::istream& in, Message& msg);


template<typename Dest>
constexpr auto ptr_cast(auto pointer) {
  using void_type = std::conditional_t<std::is_const_v<std::remove_pointer_t<decltype(pointer)>>, const void*, void*>;
  return static_cast<Dest>(static_cast<void_type>(pointer));
}
} // namespace detail

// =============================================================================
// Deserializer overloads (detail::deserialize)
// =============================================================================
namespace detail {

// --- Scalar types ---

template<variant_integral T>
[[nodiscard]] field_result deserialize(std::istream& in, T& v) {
    return { quantity::some, saturating_assign(v, read_varint(in)) };
}

template<zigzag_integral T>
[[nodiscard]] field_result deserialize(std::istream& in, T& v) {
    return { quantity::some, saturating_assign(v, decode_zigzag(read_varint(in))) };
}

template<fixed_arithmetic T>
[[nodiscard]] field_result deserialize(std::istream& in, T& v) {
    in.read(ptr_cast<char*>(&v), static_cast<std::streamsize>(sizeof(v)));
    return { quantity::some, {} };
}

// --- Wrappers ---

template<typename T>
[[nodiscard]] field_result deserialize(std::istream& in, std::optional<T>& v) {
    v.emplace();
    return deserialize(in, *v);
}

template<typename T>
[[nodiscard]] field_result deserialize(std::istream& in, std::unique_ptr<T>& v) {
    v = std::make_unique<T>();
    const auto r = deserialize(in, *v);
    return { quantity::some, r.errors };
}

// --- String types ---
template<string_like String>
[[nodiscard]] inline field_result deserialize(std::istream& in, String& v) {
    auto len = read_length(in);
    if(len > max_valid_string_length) {
      in.bad();
      return  { quantity::some, parse_error::invalid_length };
    }
    const std::size_t excessive = len > v.max_size() ? (len - v.max_size()) : 0;
    len -= excessive;
    v.resize(len);
    in.read(v.data(), static_cast<std::streamsize>(len));
    if (excessive == 0Z) return  { quantity::some, {} };
    in.ignore(static_cast<std::streamsize>(excessive));
    return  { quantity::some, parse_error::string_overrun };
}

template<std::size_t N>
[[nodiscard]] field_result deserialize(std::istream& in, std::array<char, N>& v) {
    const auto len = read_length(in);
    const auto to_read = std::min(len, N);
    in.read(v.data(), static_cast<std::streamsize>(to_read));
    if (len > N) { in.ignore(static_cast<std::streamsize>(len - N)); }
    return { quantity::some, len > N ? parse_error::array_overrun : parse_error::none };
}

template<std::size_t N>
[[nodiscard]] field_result deserialize(std::istream& in, char (&v)[N]) {
    const auto len = read_length(in);
    const auto to_read = std::min(len, N);
    in.read(v, static_cast<std::streamsize>(to_read));
    if (len > N) { in.ignore(static_cast<std::streamsize>(len - N)); }
    return { quantity::some, len > N ? parse_error::array_overrun : parse_error::none };
}

// --- Fixed-element arrays (raw binary: fixed32[], byte[], etc.)
//     More constrained than bounded_array, takes priority for fixed_scalar elements.
//     Excludes char-based arrays (handled by the overloads above).

template<fixed_array Array>
    requires (std::is_bounded_array_v<std::remove_cvref_t<Array>>
              ? !std::is_same_v<std::remove_extent_t<std::remove_cvref_t<Array>>, char>
              : !std::is_same_v<typename std::remove_cvref_t<Array>::value_type, char>)
[[nodiscard]] field_result deserialize(std::istream& in, Array& v) {
    const auto len    = read_length(in);
    const auto size   = std::size(v) * sizeof(*std::begin(v));
    const auto to_rw  = std::min(size, len);
    in.read(ptr_cast<char*>(std::begin(v)), static_cast<std::streamsize>(to_rw));
    if (len > size) { in.ignore(static_cast<std::streamsize>(len - size)); }
    return { quantity::some, { len > size ? parse_error::array_overrun : parse_error::none } };
}

// --- Contiguous resizable arrays of fixed-scalar elements (vector<byte> etc.)
//     Raw binary: length prefix then raw bytes.

template<contiguous_resizable_array Array>
[[nodiscard]] field_result deserialize(std::istream& in, Array& v) {
    const auto len   = read_length(in);
    auto count = len / sizeof(typename Array::value_type);
    const std::size_t excessive = count > v.max_size() ? (count - v.max_size()) : 0Z;
    count -= excessive;
    v.resize(count);
    in.read(ptr_cast<char*>(v.data()), static_cast<std::streamsize>(len));
    if (excessive == 0) return { quantity::some, {} };
    in.ignore(static_cast<std::streamsize>(excessive * sizeof(typename Array::value_type)));
    return { quantity::some, parse_error::array_overrun };
}

// --- Bounded arrays of varint-decodable elements (uint64_t[12], etc.)
//     Always packed (proto3 semantics for repeated scalar arrays).
//     Less constrained than fixed_array, so it is tried last.

template<bounded_array Array>
[[nodiscard]] field_result deserialize(std::istream& in, Array& v) {
    const auto eod     = get_eod(in);
    auto       current = std::begin(v);
    const auto last    = std::end(v);
    field_result result{};
    while (in.good()
           && get_pos(in) < eod
           && current != last)
    {
        std::remove_cvref_t<decltype(*current)> elem{};
        const auto [q, e] = deserialize(in, elem);
        result.errors |= e;
        if (q == quantity::some) {
            *current++ = std::move(elem);
            result.read = quantity::some;
        } else { break; }
    }
    if (get_pos(in) < eod) {
        in.ignore(static_cast<std::streamsize>(
            eod - get_pos(in)));
        result.errors |= parse_error::array_overrun;
        return result;
    }
    return result;
}

// --- Associative (map<K,V>): each invocation deserializes one entry

template<associative Container>
[[nodiscard]] field_result deserialize(std::istream& in, Container& v) {
    using Key   = std::remove_cvref_t<typename Container::key_type>;
    using Value = std::remove_cvref_t<typename Container::mapped_type>;
    MapEntry<Key, Value> entry{};
    const auto r = deserialize_message(in, entry, &get_eod);
    if (any(r)) {
        v.emplace(std::make_pair(std::move(entry.key), std::move(entry.value)));
    }
    return { any(r) ? quantity::some : quantity::none, r.errors() };
}

// --- Nested message: reads the length prefix then the message body

template<proto23::message Message>
[[nodiscard]] field_result deserialize(std::istream& in, Message& msg) {
    const auto r = deserialize_message(in, msg, &get_eod);
    return field_result { any(r) ? quantity::some : quantity::none, r.errors() };
}

// --- Empty message: reads the length prefix then ignore the message body

template<class Message>
requires (std::is_empty_v<Message>)
[[nodiscard]] field_result deserialize(std::istream& in, Message&) {
    in.ignore(static_cast<std::streamsize>(read_length(in)));
    return field_result { quantity::some, {} };
}

// =============================================================================
// Container deserialization (pack-aware)
// =============================================================================

/// container<C> with explicit packed_t: decides packed vs unpacked per element
template<packed_t Pack, container C>
[[nodiscard]] field_result deserialize_container(std::istream& in, C& v) {
    using E = std::remove_cvref_t<typename C::value_type>;
    if constexpr (unpacked<Pack, C>) {
        if (v.size() >= v.max_size()) {
          const auto [id, wt] = read_id_type(in);
          skip(in, wt);
          return { quantity::some, parse_error::array_overrun };
        }
        // Unpacked: one element per wire occurrence
        E elem{};
        const auto r = deserialize(in, elem);
        if (r.read == quantity::some) {
            v.emplace_back(std::move(elem));
            return r;
        }
        return { quantity::none, {} };
    } else {
      // Packed: length-delimited sequence of elements
      const auto eod    = get_eod(in);
      std::size_t count = 0U;
      parse_error err {};
      auto inserter     = std::back_inserter(v);
      while (in.good() && get_pos(in) < eod) {
          if (v.size() >= v.max_size()) {
            const auto [id, wt] = read_id_type(in);
            skip(in, wt);
            err |=  parse_error::array_overrun;
            continue;
          }
          E elem{};
          const auto r = deserialize(in, elem);
          if (r.read == quantity::some) {
              inserter = std::move(elem);
              ++count;
          } else { break; }
      }
      return { count == 0 ? quantity::none : quantity::some, err };
    }
}

// =============================================================================
// Field dispatchers (try_field / try_oneof_alt / try_field_at)
// =============================================================================

// Regular Field<M,N,P>
template<typename FD, std::size_t Idx, typename Msg>
[[nodiscard]] try_field_result try_field(std::istream& in, Msg& msg, int num, data_type wt)
{
    using VType = typename FD::value_type;      // unwrapped (optional<T> -> T)
    using AType = std::remove_cvref_t<
        decltype(msg.*FD::member)>;             // actual field type

    if (num != FD::num) {
      return {{quantity::none, {}}, try_field_result::fail };
    }
    auto& field_ref = msg.*FD::member;
    if (!matches<std::remove_cvref_t<VType>>(wt)) {
        if constexpr(packable<VType>) {
            if constexpr(unpacked<FD::packing, VType>) {
                if (matches<std::remove_cvref_t<typename VType::value_type>>(wt)) {
                  return { deserialize_container<FD::packing>(in, field_ref), Idx };
                } else {
                  return {{ quantity::none, parse_error::type_mismatch}, try_field_result::fail };
                }
             }
      } else {
        return {{ quantity::none, parse_error::type_mismatch}, try_field_result::fail };
      }
    }

    // Containers of non-binary elements: pack-aware dispatch
    if constexpr (container<AType> && !contiguous_resizable_array<AType>) {
        return { deserialize_container<FD::packing>(in, field_ref), Idx };
    } else {
        // Covers: scalar, string, char[], array<char,N>, fixed_array,
        //         contiguous_resizable_array, bounded_array,
        //         optional<T>, unique_ptr<T>, message, associative
        return { detail::deserialize(in, field_ref), Idx };
    }
}

// OneOf<M, Ns...>: try each variant alternative in turn
template<typename OD, std::size_t Idx, std::size_t AltI, typename Msg>
[[nodiscard]] try_field_result try_oneof_alt(std::istream& in, Msg& msg,
                                         int num, data_type wt)
{
    using VType = typename OD::value_type;    // std::variant<...>
    if constexpr (AltI < std::variant_size_v<VType>) {
        using Alt = std::variant_alternative_t<AltI, VType>;
        if (num == OD::nums[AltI] && matches<Alt>(wt)) {
            auto& var = msg.*OD::member;
            var.template emplace<AltI>();
            return { deserialize(in, std::get<AltI>(var)), Idx };
        }
        return try_oneof_alt<OD, Idx, AltI + 1U>(in, msg, num, wt);
    }
    return {{ quantity::none, {}}, try_field_result::fail };
}

// Walk the fields_tuple at compile time
template<typename Model, std::size_t I, typename Msg>
[[nodiscard]] try_field_result try_field_at(std::istream& in, Msg& msg,
                                        int num, data_type wt)
{
    if constexpr (I < Model::field_count) {
        using FD = std::tuple_element_t<I, typename Model::fields_tuple>;

        try_field_result result {{}, try_field_result::fail};
        if constexpr (is_oneof_desc<FD>::value) {
            result = try_oneof_alt<FD, I, 0U>(in, msg, num, wt);
        } else {
            result = try_field<FD, I>(in, msg, num, wt);
        }
        if ((result.errors & parse_error::type_mismatch) != parse_error::none) {
          return result;
        }
        if (result.read != quantity::none) {
            return result;
        }
        return try_field_at<Model, I + 1U>(in, msg, num, wt);
    }
    return {{ quantity::none, {} }, Model::field_count + 1 };
}

// =============================================================================
// Core message deserializer loop
// =============================================================================

template<proto23::message Message>
[[nodiscard]] deserialize_result<Message>
deserialize_message(std::istream& in, Message& msg,
                    length_type (*eod_fn)(std::istream&))
{
    using Model = model<Message>;
    deserialize_result<Message> result{};
    const auto end_pos = eod_fn(in);
    while (in.good() && get_pos(in) < end_pos)
    {
        const auto [id, wt] = read_id_type(in);
        if (!in.good()) { break; }
        const auto res = try_field_at<Model, 0U>(in, msg, id, wt);
        if (res.read == quantity::none) {
          skip(in, wt); // Extra fields is an anticipated condition. Skip with no error
        }
        result.set(res.id, res.errors);
    }
    return result;
}

} // namespace detail

// =============================================================================
// Public deserialize API
// =============================================================================

/// Deserialize a top-level message from a stream (no length prefix).
/// Returns a Result tracking which fields were present.
template<message Message>
[[nodiscard]] deserialize_result<Message>
deserialize(std::istream& in, Message& msg) {
    if (in.tellg() == -1) return deserialize_result<Message>{parse_error::tellg_fail};
    return detail::deserialize_message(in, msg, &detail::unlimited);
}

// =============================================================================
// Serializer overloads (detail::serialize_value / serialize_field_at)
// =============================================================================
namespace detail {

// Speculative buffer size for length-delimited serialization.
// If the encoded payload fits, it is written directly from the buffer.
// If it overflows, the accumulated size is used as the length prefix and
// the payload is re-serialized directly to the output stream.
template<class>
constexpr std::size_t inplace_buf_size = 256;

enum class skippable : bool { no, yes };

inline constexpr bool operator&&(skippable a, bool b) {
  return static_cast<bool>(a) && b;
}

// ---- Varint scalar ----

template<variant_integral T>
void serialize_value(std::ostream& out, int num, T v, skippable mayskip) {
    std::uint64_t raw{};
    if(mayskip && v == T{}) return;
    if constexpr (std::is_same_v<T, bool>) {
        raw = v ? 1U : 0U;
    } else if constexpr (std::is_enum_v<T>) {
        using UT = std::underlying_type_t<T>;
        if constexpr (std::is_signed_v<UT>) {
            // Proto int32/int64: sign-extend to int64 so negatives use 10-byte varint
            raw = static_cast<std::uint64_t>(
                      static_cast<std::int64_t>(static_cast<UT>(v)));
        } else {
            raw = static_cast<std::uint64_t>(static_cast<UT>(v));
        }
    } else {
        raw = static_cast<std::uint64_t>(v);
    }
    write_id_type(out, num, data_type::variant);
    write_varint(out, raw);
}

// ---- Zigzag scalar ----

template<zigzag_integral T>
void serialize_value(std::ostream& out, int num, T v, skippable mayskip) {
    if(mayskip && v == T{}) return;
    const auto encoded = encode_zigzag(v);
    write_id_type(out, num, data_type::variant);
    write_varint(out, encoded);
}

// ---- Fixed-width scalar ----

template<fixed_arithmetic T>
void serialize_value(std::ostream& out, int num, T v, skippable mayskip) {
  if(mayskip && v == T{}) return;
  constexpr auto wt = (sizeof(T) == sizeof(proto23::fixed32)) ? data_type::fixed32 : data_type::fixed64;
  write_id_type(out, num, wt);
  out.write(ptr_cast<const char*>(&v), static_cast<std::streamsize>(sizeof(T)));
}

// ---- String ----
template<string_like String>
void serialize_value(std::ostream& out, int num, const String& v, skippable mayskip) {
    if (mayskip && v.empty()) return;
    write_id_type(out, num, data_type::lengthy);
    write_length(out, v.size());
    out.write(v.data(), static_cast<std::streamsize>(v.size()));
}

// ---- char[N] (null-terminated string) ----

template<std::size_t N>
void serialize_value(std::ostream& out, int num, const char (&v)[N], skippable mayskip) {
    const auto len = ::strnlen(v, N);
    if (mayskip && len == 0U) return;
    write_id_type(out, num, data_type::lengthy);
    write_length(out, len);
    out.write(v, static_cast<std::streamsize>(len));
}

// ---- std::array<char,N> (null-terminated string) ----

template<std::size_t N>
void serialize_value(std::ostream& out, int num, const std::array<char, N>& v, skippable mayskip) {
    const std::string_view sv{v.data(), N};
    const auto nul_pos = sv.find('\0');
    const auto len = (nul_pos == std::string_view::npos) ? N : nul_pos;
    if (mayskip && len == 0U) return;
    write_id_type(out, num, data_type::lengthy);
    write_length(out, len);
    out.write(v.data(), static_cast<std::streamsize>(len));
}

// ---- fixed_array (fixed32[], sfixed64[], byte[], ...) — raw binary, length-prefixed
//      More constrained than bounded_array; char-based arrays handled above.

template<fixed_array Array>
void serialize_value(std::ostream& out, int num, const Array& v, skippable mayskip) {
    const auto byte_count = std::size(v) * sizeof(*std::begin(v));
    if (mayskip && byte_count == 0U) return;
    write_id_type(out, num, data_type::lengthy);
    write_length(out, byte_count);
    out.write(ptr_cast<const char*>(std::begin(v)), static_cast<std::streamsize>(byte_count));
}

// ---- contiguous_resizable_array (vector<byte>, vector<float>, ...) — raw binary

template<contiguous_resizable_array Array>
void serialize_value(std::ostream& out, int num, const Array& v, skippable mayskip) {
    if (mayskip && v.empty()) return;
    const auto byte_count = v.size() * sizeof(typename Array::value_type);
    write_id_type(out, num, data_type::lengthy);
    write_length(out, byte_count);
    out.write(ptr_cast<const char*>(v.data()), static_cast<std::streamsize>(byte_count));
}

/// Forward declaration so optional<T>/unique_ptr<T> overloads can call it.
template<proto23::message Msg>
void serialize_value(std::ostream& out, int num, const Msg& msg, skippable mayskip);

/// Empty messages
template<class Msg>
requires (std::is_empty_v<Msg>)
void serialize_value(std::ostream& out, int num, const Msg&, skippable mayskip) {
    if (mayskip == skippable::yes) return;
    write_id_type(out, num, data_type::lengthy);
    write_length(out, 0);
}


// ---- optional<T> ----

template<typename T>
void serialize_value(std::ostream& out, int num, const std::optional<T>& v, skippable mayskip) {
    if (mayskip && !v.has_value()) return;
    serialize_value(out, num, *v, mayskip);
}

// ---- unique_ptr<T> ----

template<typename T>
void serialize_value(std::ostream& out, int num, const std::unique_ptr<T>& v, skippable mayskip) {
    if (mayskip && !v) return;
    serialize_value(out, num, *v, mayskip);
}

// ---- Nested message (length-prefixed) ----

template<proto23::message Msg>
void serialize_value(std::ostream& out, int num, const Msg& msg, skippable mayskip) {
    proto23::inplace_buffer<inplace_buf_size<Msg>> buf;
    std::ostream tmp(&buf);
    proto23::serialize(tmp, msg);
    if (mayskip && buf.size() == 0) return;
    write_id_type(out, num, data_type::lengthy);
    write_length(out, buf.size());
    if (!buf.overflowed()) {
        out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    } else {
        proto23::serialize(out, msg);
    }
}

// ---- associative (map) — each entry as embedded message ----

template<associative Container>
void serialize_value(std::ostream& out, int num, const Container& v, skippable) {
    for (const auto& [k, mv] : v) {
        using Key   = std::remove_cvref_t<typename Container::key_type>;
        using Value = std::remove_cvref_t<typename Container::mapped_type>;
        MapEntry<Key, Value> entry{k, mv};
        proto23::inplace_buffer<inplace_buf_size<Container>> buf;
        std::ostream tmp(&buf);
        proto23::serialize(tmp, entry);
        write_id_type(out, num, data_type::lengthy);
        write_length(out, buf.size());
        if (!buf.overflowed()) {
            out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
        } else {
            proto23::serialize(out, entry);
        }
    }
}

// =============================================================================
// Bounded-array serialization (varint/zigzag packed sequence)
// =============================================================================

template<bounded_array Array>
    requires (!fixed_array<Array>)
void serialize_bounded_array(std::ostream& out, int num, const Array& v) {
    if (std::size(v) == 0U) { return; }
    auto write_elem = [](std::ostream& s, const auto& elem) {
        using E = std::remove_cvref_t<decltype(elem)>;
        if constexpr (zigzag_integral<E>) {
            write_varint(s, encode_zigzag(elem));
        } else if constexpr (variant_integral<E>) {
            std::uint64_t raw{};
            if constexpr (std::is_enum_v<E>) {
                using UT = std::underlying_type_t<E>;
                if constexpr (std::is_signed_v<UT>) {
                    raw = static_cast<std::uint64_t>(
                              static_cast<std::int64_t>(static_cast<UT>(elem)));
                } else {
                    raw = static_cast<std::uint64_t>(static_cast<UT>(elem));
                }
            } else {
                raw = static_cast<std::uint64_t>(elem);
            }
            write_varint(s, raw);
        }
    };
    proto23::inplace_buffer<inplace_buf_size<Array>> buf;
    std::ostream tmp(&buf);
    for (const auto& elem : v) { write_elem(tmp, elem); }
    write_id_type(out, num, data_type::lengthy);
    write_length(out, buf.size());
    if (!buf.overflowed()) {
        out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    } else {
        for (const auto& elem : v) { write_elem(out, elem); }
    }
}

// =============================================================================
// Container serialization (packed / unpacked)
// =============================================================================

/// Write packed raw element (varint or fixed) to buf — used by serialize_container
template<typename E>
void write_packed_elem(std::ostream& buf, const E& elem) {
    if constexpr (fixed_arithmetic<E>) {
        buf.write(ptr_cast<const char*>(&elem), static_cast<std::streamsize>(sizeof(E)));
    } else if constexpr (zigzag_integral<E>) {
        write_varint(buf, encode_zigzag(elem));
    } else {
        // variant_integral
        std::uint64_t raw{};
        if constexpr (std::is_same_v<E, bool>) {
            raw = elem ? 1U : 0U;
        } else if constexpr (std::is_enum_v<E>) {
            using UT = std::underlying_type_t<E>;
            if constexpr (std::is_signed_v<UT>) {
                raw = static_cast<std::uint64_t>(
                          static_cast<std::int64_t>(static_cast<UT>(elem)));
            } else {
                raw = static_cast<std::uint64_t>(static_cast<UT>(elem));
            }
        } else {
            raw = static_cast<std::uint64_t>(elem);
        }
        write_varint(buf, raw);
    }
}

template<packed_t Pack, container C>
void serialize_container(std::ostream& out, int num, const C& v) {
    if (v.empty()) { return; }

    if constexpr (unpacked<Pack, C>) {
        // Unpacked: one tag per element
        for (const auto& elem : v) { serialize_value(out, num, elem, skippable::no); }
    } else {
        // Packed: one tag, then all elements
        proto23::inplace_buffer<inplace_buf_size<C>> buf; // FIXME packed array should have a computable size
        std::ostream tmp(&buf);
        for (const auto& elem : v) { write_packed_elem(tmp, elem); }
        write_id_type(out, num, data_type::lengthy);
        write_length(out, buf.size());
        if (!buf.overflowed()) {
            out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
        } else {
            for (const auto& elem : v) { write_packed_elem(out, elem); }
        }
    }
}

// =============================================================================
// Per-field serialization dispatch (serialize_field_at)
// =============================================================================

// OneOf: serialize the active alternative
template<typename OD, std::size_t AltI, typename Msg>
void serialize_oneof(std::ostream& out, const Msg& msg) {
    using VType = typename OD::value_type;
    if constexpr (AltI < std::variant_size_v<VType>) {
        using Alt = std::variant_alternative_t<AltI, VType>;
        const auto& var = msg.*OD::member;
        if (std::holds_alternative<Alt>(var)) {
            serialize_value(out, OD::nums[AltI], std::get<AltI>(var), skippable::yes );
        }
        serialize_oneof<OD, AltI + 1U>(out, msg);
    }
}

template<typename Model, std::size_t I, typename Msg>
void serialize_field_at(std::ostream& out, const Msg& msg) {
    if constexpr (I < Model::field_count) {
        using FD = std::tuple_element_t<I, typename Model::fields_tuple>;

        if constexpr (is_oneof_desc<FD>::value) {
            serialize_oneof<FD, 0U>(out, msg);
        } else {
            using AType = std::remove_cvref_t<decltype(msg.*FD::member)>;
            const auto& field_ref = msg.*FD::member;
            constexpr int num = FD::num;

            if constexpr (container<AType>) {
                // Vectors, lists — pack-aware
                serialize_container<FD::packing>(out, num, field_ref);
            } else if constexpr (bounded_array<AType> && !fixed_array<AType>) {
                // Fixed-size arrays of varint elements (uint64_t[12] etc.)
                serialize_bounded_array(out, num, field_ref);
            } else {
                // Everything else: scalar, string, char[], array<char,N>,
                // fixed_array, contiguous_resizable_array,
                // optional, unique_ptr, message, associative
                serialize_value(out, num, field_ref, skippable::yes);
            }
        }
    }
}

template<message Message, std::size_t ... I>
void serialize_fields(std::ostream& out, const Message& msg, std::index_sequence<I...>) {
  (serialize_field_at<model<Message>, I>(out, msg), ...);
}

} // namespace detail
// =============================================================================
// Public serialize API
// =============================================================================

/// Serialize a message to the stream (no length prefix).
template<message Message>
void serialize(std::ostream& out, const Message& msg) {
  detail::serialize_fields(out, msg, std::make_index_sequence<model<Message>::field_count>());
}

// =============================================================================
// Convenience operator+ for converting proto23 enum to underlying integers
// =============================================================================

namespace operators {
template<proto23::detail::numeric_enum Enum>
constexpr auto operator+(Enum value) {
    return static_cast<std::underlying_type_t<Enum>>(value);
}
} // namespace operators

} // namespace proto23
