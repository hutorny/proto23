/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * proto23/test/data.h - Test data structures for C++23 proto serializer.
 *
 * Licensed under MIT License, see full text in LICENSE
 */
#pragma once
#include <proto23/proto23.h>
#include <array>
#include <beman/inplace_vector/inplace_vector.hpp>
#include <cstddef>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace testing {

using namespace proto23;                          // bring wire types into scope
using namespace beman::inplace_vector;

// ---------------------------------------------------------------------------
// Enum used across test messages
// ---------------------------------------------------------------------------
enum class Status : std::int32_t {
    NOTOK = 0,
    OK    = 42,
};

// ---------------------------------------------------------------------------
// Scalar-only message
// ---------------------------------------------------------------------------
struct Simple {
    std::int32_t  a{};
    std::uint32_t b{};
    sint32        c{};
    int32         d{};
    fixed32       e{};
    Status        status{};

    using Model = Fields<
        Field<&Simple::a,      1>,
        Field<&Simple::b,      2>,
        Field<&Simple::c,      3>,
        Field<&Simple::d,      4>,
        Field<&Simple::e,      5>,
        Field<&Simple::status, 6>>;
};

// ---------------------------------------------------------------------------
// Complex nested message (forward-declared for recursive pointer)
// ---------------------------------------------------------------------------
struct Complex {
    Simple                          simple{};
    std::optional<std::int32_t>     opt{};
    std::vector<double>             values{};
    std::uint64_t                   arr[12]{};
    std::map<int, std::string>      map{};
    std::string                     str{};
    std::unique_ptr<Complex>        complex{};
    fixed32                         fix[16]{};
    char                            text[64]{};
    std::array<char, 32>            msg{};

    using Model = Fields<
        Field<&Complex::simple,  1>,
        Field<&Complex::opt,     2>,
        Field<&Complex::values,  3>,
        Field<&Complex::arr,     4>,
        Field<&Complex::map,     5>,
        Field<&Complex::str,     6>,
        Field<&Complex::complex, 11>,
        Field<&Complex::fix,     12>,
        Field<&Complex::text,    13>,
        Field<&Complex::msg,     14>>;
};

// ---------------------------------------------------------------------------
// Aggregate with repeated nested message
// ---------------------------------------------------------------------------
struct Aggregate {
    Complex              complex{};
    std::vector<Complex> complexes{};

    using Model = Fields<
        Field<&Aggregate::complex,             1>,
        UnpackedField<&Aggregate::complexes,   2>>;
};

// ---------------------------------------------------------------------------
// Oneof — anonymous union style
// ---------------------------------------------------------------------------
struct OneOfUnion {
    union {
        double value;
        int    count;
    };

    using Model = Fields<
        Field<&OneOfUnion::value, 2>,
        Field<&OneOfUnion::count, 3>>;
};

// ---------------------------------------------------------------------------
// Oneof — std::variant style
// ---------------------------------------------------------------------------
struct OneOfVariant {
    std::variant<double, int> oneof{};

    using Model = Fields<
        OneOf<&OneOfVariant::oneof, 2, 3>>;
};

// ---------------------------------------------------------------------------
// Single-scalar test structs (mirror proto-data.h)
// ---------------------------------------------------------------------------
struct Bool_     { bool    v{};  bool operator==(const Bool_&)    const = default; using Model = Fields<Field<&Bool_::v,    1>>; };
struct Int_32    { int32   v{};  bool operator==(const Int_32&)   const = default; using Model = Fields<Field<&Int_32::v,   1>>; };
struct SInt_32   { sint32  v{};  bool operator==(const SInt_32&)  const = default; using Model = Fields<Field<&SInt_32::v,  1>>; };
struct UInt_32   { uint32  v{};  bool operator==(const UInt_32&)  const = default; using Model = Fields<Field<&UInt_32::v,  1>>; };
struct Fixed_32  { fixed32 v{};  bool operator==(const Fixed_32&) const = default; using Model = Fields<Field<&Fixed_32::v, 1>>; };
struct SFixed_32 { sfixed32 v{}; bool operator==(const SFixed_32&)const = default; using Model = Fields<Field<&SFixed_32::v,1>>; };
struct Int_64    { int64   v{};  bool operator==(const Int_64&)   const = default; using Model = Fields<Field<&Int_64::v,   1>>; };
struct SInt_64   { sint64  v{};  bool operator==(const SInt_64&)  const = default; using Model = Fields<Field<&SInt_64::v,  1>>; };
struct UInt_64   { uint64  v{};  bool operator==(const UInt_64&)  const = default; using Model = Fields<Field<&UInt_64::v,  1>>; };
struct Fixed_64  { fixed64 v{};  bool operator==(const Fixed_64&) const = default; using Model = Fields<Field<&Fixed_64::v, 1>>; };
struct SFixed_64 { sfixed64 v{}; bool operator==(const SFixed_64&)const = default; using Model = Fields<Field<&SFixed_64::v,1>>; };
struct Float_    { float   v{};  bool operator==(const Float_&)   const = default; using Model = Fields<Field<&Float_::v,   1>>; };
struct Double_   { double  v{};  bool operator==(const Double_&)  const = default; using Model = Fields<Field<&Double_::v,  1>>; };

// ---------------------------------------------------------------------------
// Two-field test structs
// ---------------------------------------------------------------------------
struct Bool_SInt {
    bool v{};
    int  k{};
    bool operator==(const Bool_SInt&) const = default;
    using Model = Fields<Field<&Bool_SInt::v, 2>, Field<&Bool_SInt::k, 3>>;
};

struct SInt_String {
    int         k{};
    std::string v{};
    bool operator==(const SInt_String&) const = default;
    using Model = Fields<Field<&SInt_String::k, 4>, Field<&SInt_String::v, 5>>;
};

struct UInt_Fixed {
    unsigned k{};
    fixed32  v{};
    bool operator==(const UInt_Fixed&) const = default;
    using Model = Fields<Field<&UInt_Fixed::k, 6>, Field<&UInt_Fixed::v, 8>>;
};

struct Double_SFixed {
    double   k{};
    sfixed32 v{};
    using Model = Fields<Field<&Double_SFixed::v, 1>, Field<&Double_SFixed::k, 11>>;
};

// ---------------------------------------------------------------------------
// Repeated scalar test structs
// ---------------------------------------------------------------------------
struct Bool_v {
  bool v[5] { };
  using Model = Fields<Field<&Bool_v::v, 1>>;
};
struct Int_32v {
  std::vector<int32> v { };
  bool operator==(const Int_32v&)  const = default;
  using Model = Fields<Field<&Int_32v::v, 1>>;
};
struct SInt_32v {
  std::list<int> v { };
  bool operator==(const SInt_32v&)  const = default;
  using Model = Fields<Field<&SInt_32v::v, 1>>;
};
struct UInt_32v {
  std::vector<uint32> v { };
  bool operator==(const UInt_32v&)  const = default;
  using Model = Fields<Field<&UInt_32v::v, 1>>;
};
struct Fixed_32v {
  fixed32 v[5] { };
  bool operator==(const Fixed_32v&)  const = default;
  using Model = Fields<Field<&Fixed_32v::v, 1>>;
};
struct SFixed_32v {
  std::array<sfixed32, 5> v { };
  bool operator==(const SFixed_32v&)  const = default;
  using Model = Fields<Field<&SFixed_32v::v,1>>;
};
struct Int_64v {
  std::vector<int64> v { };
  bool operator==(const Int_64v&)  const = default;
  using Model = Fields<Field<&Int_64v::v, 1>>;
};
struct SInt_64v {
  std::list<std::int64_t> v { };
  bool operator==(const SInt_64v&)  const = default;
  using Model = Fields<Field<&SInt_64v::v, 1>>;
};
struct UInt_64v {
  std::array<uint64, 5> v { };
  bool operator==(const UInt_64v&)  const = default;
  using Model = Fields<Field<&UInt_64v::v, 1>>;
};
struct Fixed_64v {
  std::vector<fixed64> v { };
  bool operator==(const Fixed_64v&)  const = default;
  using Model = Fields<Field<&Fixed_64v::v, 1>>;
};
struct SFixed_64v {
  std::vector<sfixed64> v { };
  bool operator==(const SFixed_64v&)  const = default;
  using Model = Fields<Field<&SFixed_64v::v,1>>;
};
struct Float_v {
  float v[5] { };
  bool operator==(const Float_v&)  const = default;
  using Model = Fields<Field<&Float_v::v, 1>>;
};
struct Double_v {
  std::vector<double> v { };
  bool operator==(const Double_v&)  const = default;
  using Model = Fields<Field<&Double_v::v, 1>>;
};

// ---------------------------------------------------------------------------
// Repeated unpacked scalar test structs
// ---------------------------------------------------------------------------

struct Bool_u {
  inplace_vector<bool, 5> v { };
  using Model = Fields<Field<&Bool_u::v, 1, packed_t::no>>;
};
struct Int_32u {
  std::vector<int32> v { };
  using Model = Fields<Field<&Int_32u::v, 1, packed_t::no>>;
};
struct SInt_32u {
  std::list<int> v { };
  using Model = Fields<Field<&SInt_32u::v, 1, packed_t::no>>;
};
struct UInt_32u {
  std::vector<uint32> v { };
  using Model = Fields<Field<&UInt_32u::v, 1, packed_t::no>>;
};
struct Fixed_32u {
  std::vector<fixed32> v { };
  using Model = Fields<Field<&Fixed_32u::v, 1, packed_t::no>>;
};
struct SFixed_32u {
  std::vector<sfixed32> v { };
  using Model = Fields<Field<&SFixed_32u::v,1, packed_t::no>>;
};
struct Int_64u {
  std::vector<int64> v { };
  using Model = Fields<Field<&Int_64u::v, 1, packed_t::no>>;
};
struct SInt_64u {
  std::list<std::int64_t> v { };
  using Model = Fields<Field<&SInt_64u::v, 1, packed_t::no>>;
};
struct UInt_64u {
  inplace_vector<uint64, 5> v { };
  using Model = Fields<Field<&UInt_64u::v, 1, packed_t::no>>;
};
struct Fixed_64u {
  std::vector<fixed64> v { };
  using Model = Fields<Field<&Fixed_64u::v, 1, packed_t::no>>;
};
struct SFixed_64u {
  std::vector<sfixed64> v { };
  using Model = Fields<Field<&SFixed_64u::v, 1, packed_t::no>>;
};
struct Float_u {
  inplace_vector<float, 5> v { };
  using Model = Fields<Field<&Float_u::v, 1, packed_t::no>>;
};
struct Double_u {
  std::vector<double> v { };
  using Model = Fields<Field<&Double_u::v, 1, packed_t::no>>;
};

// ---------------------------------------------------------------------------
// Nested message test structs
// ---------------------------------------------------------------------------
struct Inner {
    Status status{};
    int    v{};
    using Model = Fields<Field<&Inner::status, 1>, Field<&Inner::v, 2>>;
};

struct Outer {
    Inner inner{};
    int   v{};
    using Model = Fields<Field<&Outer::inner, 1>, Field<&Outer::v, 2>>;
};

struct Combined {
    struct Local {
        int         v{};
        std::string text{};
        using Model = Fields<Field<&Local::v, 1>, Field<&Local::text, 2>>;
    };
    Inner inner{};
    Outer outer{};
    Local local{};
    int   v{};
    using Model = Fields<
        Field<&Combined::inner, 1>,
        Field<&Combined::outer, 2>,
        Field<&Combined::local, 3>,
        Field<&Combined::v,     4>>;
};

// ---------------------------------------------------------------------------
// String / bytes test structs
// ---------------------------------------------------------------------------
struct String       { std::string           v{};   bool operator==(const String&)     const = default; using Model = Fields<Field<&String::v,     1>>; };
struct Chars        { char                  v[256]{};bool operator==(const Chars& o)    const noexcept { return std::memcmp(v, o.v, sizeof(v)) == 0; } using Model= Fields<Field<&Chars::v,      1>>; };
struct CharArray    { std::array<char, 256> v{};   bool operator==(const CharArray&)   const = default; using Model = Fields<Field<&CharArray::v,   1>>; };
struct Bytes        { std::vector<std::byte>v{};   bool operator==(const Bytes&)       const = default; using Model = Fields<Field<&Bytes::v,       1>>; };
// FIXME size of
struct ByteArray    { std::array<std::byte,12>v{};bool operator==(const ByteArray&)   const = default; using Model = Fields<Field<&ByteArray::v,   1>>; };

struct ExcessiveChars {
    char    v[16]{};
    fixed32 excessive{};
    using Model = Fields<Field<&ExcessiveChars::v, 1>, Field<&ExcessiveChars::excessive, 2>>;
};

struct ExcessiveArray {
    std::array<char, 16> v{};
    fixed32              excessive{};
    using Model = Fields<Field<&ExcessiveArray::v, 1>, Field<&ExcessiveArray::excessive, 2>>;
};

// ---------------------------------------------------------------------------
// Union / Variant oneof
// ---------------------------------------------------------------------------
struct Union {
    int v{};
    union {
        double value{};
        int    count;
    };
    using Model = Fields<
        Field<&Union::v,     1>,
        Field<&Union::value, 2>,
        Field<&Union::count, 3>>;
};

struct Variant {
    int v{};
    std::variant<double, int> value{};
    bool operator==(const Variant&) const = default;
    using Model = Fields<
        Field<&Variant::v,   1>,
        OneOf<&Variant::value, 2, 3>>;
};

// ---------------------------------------------------------------------------
// Optional / unique_ptr
// ---------------------------------------------------------------------------
struct Optional {
    std::optional<Inner> inner{};
    std::optional<int>   v{};
    using Model = Fields<Field<&Optional::inner, 1>, Field<&Optional::v, 2>>;
};

struct Pointer {
    std::unique_ptr<Inner> inner{};
    int                    v{};
    using Model = Fields<Field<&Pointer::inner, 1>, Field<&Pointer::v, 2>>;
};

// ---------------------------------------------------------------------------
// Map test structs
// ---------------------------------------------------------------------------
struct Map {
    int                        v{};
    std::map<int, std::string> dict{};
    using Model = Fields<Field<&Map::v, 1>, Field<&Map::dict, 2>>;
};

struct UnorderedMap {
    int                                  v{};
    std::unordered_map<int, std::string> dict{};
    using Model = Fields<Field<&UnorderedMap::v, 1>, Field<&UnorderedMap::dict, 2>>;
};

struct ExtModel {
    std::int32_t  a{};
    std::uint32_t b{};
    sint32        c{};
    int32         d{};
    fixed32       e{};
    Status        status{};
};

} // namespace testing

namespace proto23 {
template<>
struct external_model<testing::ExtModel> {
  using type = Fields<
    Field<&testing::ExtModel::a,      1>,
    Field<&testing::ExtModel::b,      2>,
    Field<&testing::ExtModel::c,      3>,
    Field<&testing::ExtModel::d,      4>,
    Field<&testing::ExtModel::e,      5>,
    Field<&testing::ExtModel::status, 6>>;
};
}
