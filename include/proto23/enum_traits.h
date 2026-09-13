/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * proto23/enum_traits.h - Protobuf Enum traits for the generated code
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
#include <type_traits>

namespace proto23 {

template<typename T>
concept enum_type = std::is_enum_v<T>; // differs from proto23::detail::enumeration

template<enum_type E>
struct enum_traits;

namespace detail {

template<enum_type E, int Min, int Max, std::size_t Count, unsigned Mask, bool Sequence>
struct make_enum_traits {
    static constexpr auto min() noexcept { return static_cast<E>(Min); }
    static constexpr auto max() noexcept { return static_cast<E>(Max); }
    static constexpr auto count() noexcept { return Count; }
    static constexpr auto mask() noexcept { return Mask; }
    static constexpr bool is_sequence = Sequence;
};
} // namespace detail

} // namespace proto23
