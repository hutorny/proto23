#pragma once
#include <boost/ut.hpp>
#include <proto23/proto23.h>
#include <meta/nameof.h>
#include "data.h"

#include <string_view>
#include <string>
#include <source_location>

namespace testing {

using namespace std;
using namespace proto23;

static_assert(message<Inner>);

template<message Message, class Visitor, std::size_t I>
void visit_model_element(const Message& msg, Visitor& visitor) {
  using field_type = std::tuple_element_t<I, typename Message::Model::fields_tuple>;
  visitor(msg.*field_type::member);
}

template<message Message, class Visitor, std::size_t ... I>
void visit_model_element(const Message& msg, Visitor& visitor, std::index_sequence<I...>) {
  (visit_model_element<Message, Visitor, I>(msg, visitor), ...);
}

template<message Message, class Visitor>
void visit_model(const Message& msg, Visitor& visitor) {
  visit_model_element<Message, Visitor>(msg, visitor, std::make_index_sequence<Message::Model::field_count>{});
}

template<typename Collection>
concept collection = detail::container<Collection> || detail::bounded_array<Collection>;

inline constexpr auto tostr(bool value) { return value ? "true"s : "false"s; }
inline constexpr auto tostr(const string& value) { return value; }
constexpr auto tostr(detail::arithmetic auto value) requires is_enum_v<decltype(value)> {
  return to_string(static_cast<underlying_type_t<decltype(value)>>(value));
}
constexpr auto tostr(auto value) requires is_pointer_v<decltype(value)> { return tostr(*value); }
constexpr auto tostr(collection auto value) { return tostr(*begin(value)); }
constexpr auto tostr(detail::associative auto value) { return tostr(begin(value)->first); }
constexpr auto tostr(auto value) { return to_string(value); }
template<typename Type>
constexpr auto tostr(const std::optional<Type>& value) { return value ? tostr(*value) : ""s; }

constexpr auto& operator<<(std::ostream& out, detail::enumeration auto value) {
  return out << tostr(value);
}

template<typename FieldDescr>
constexpr bool compare_field_values(const typename FieldDescr::class_type& a, const typename FieldDescr::class_type& b) {
    if constexpr (std::is_bounded_array_v<typename FieldDescr::value_type>) {
      return std::equal(std::begin(a.*FieldDescr::member), std::end(a.*FieldDescr::member), std::begin(b.*FieldDescr::member));
    } else {
      return a.*FieldDescr::member == b.*FieldDescr::member;
    }
}

template<message Message, std::size_t I = 0>
constexpr bool compare_fields(const Message& a, const Message& b) {
  using FT = typename Message::Model::fields_tuple;
  if constexpr(I < std::tuple_size_v<FT>) {
    using FD = std::tuple_element_t<I, FT>;
    return compare_field_values<FD>(a, b) && compare_fields<Message, I+1>(a, b);
  } else {
    return true;
  }
}

template<message Message>
constexpr bool operator==(const Message& a, const Message& b) {
  return compare_fields(a, b);
}

template<detail::associative Map>
auto& operator<<(std::ostream& out, const Map& value) {
  char dlm = '{';
  for(auto v : value) {
    out << dlm << v.first << "->" << v.second;
    dlm = ',';
  }
  if (dlm == ',') out << '}';
  return out;
}

template<collection Container>
requires (!same_as<typename Container::value_type, char>)
constexpr auto& operator<<(std::ostream& out, const Container& container) {
  char dlm = '[';
  for(auto value : container) {
    out << dlm << value;
    dlm = ',';
  }
  if (dlm == ',') out << ']';
  return out;
}

template<typename ... Type>
auto& operator<<(std::ostream& out, const std::variant<Type...>& value) {
  std::visit([&o = out](auto v) mutable { o << v; }, value);
  return out;
}

template<typename Type>
auto& operator<<(std::ostream& out, const std::optional<Type>& value) {
  if (value) out << *value;
  return out;
}

template<std::size_t N>
auto& operator<<(std::ostream& out, const std::array<char, N>& value) {
  return out << string_view(begin(value), end(value));
}

class ostream_printer {
public:
  explicit ostream_printer(std::ostream& out) : out_ { out } {}
  ostream_printer(const ostream_printer&) = delete;
  ostream_printer(ostream_printer&&) = delete;
  ostream_printer& operator=(const ostream_printer&) = delete;
  ostream_printer& operator=(ostream_printer&&) = delete;
  ~ostream_printer() { if (dlm == ',') out_ << '}'; }
  void operator()(auto&& value) {
    out_ << dlm << value;
    dlm = ',';
  }
private:
  std::ostream& out_;
  char dlm = '{';
};

template<message Message>
auto& operator<<(std::ostream& out, const Message& msg) {
  ostream_printer printer{out};
  visit_model(msg, printer);
  return out;
}

[[maybe_unused]] inline constexpr auto operator""_i32(unsigned long long int value) { return int32(value); }
[[maybe_unused]] inline constexpr auto operator""_s32(unsigned long long int value) { return sint32(value); }
[[maybe_unused]] inline constexpr auto operator""_u32(unsigned long long int value) { return uint32(value); }
[[maybe_unused]] inline constexpr auto operator""_f32(unsigned long long int value) { return fixed32(value); }
[[maybe_unused]] inline constexpr auto operator""_sf32(unsigned long long int value) { return sfixed32(value); }
[[maybe_unused]] inline constexpr auto operator""_i64(unsigned long long int value) { return int64(value); }
[[maybe_unused]] inline constexpr auto operator""_s64(unsigned long long int value) { return sint64(value); }
[[maybe_unused]] inline constexpr auto operator""_u64(unsigned long long int value) { return uint64(value); }
[[maybe_unused]] inline constexpr auto operator""_f64(unsigned long long int value) { return fixed64(value); }
[[maybe_unused]] inline constexpr auto operator""_sf64(unsigned long long int value) { return sfixed64(value); }

template<typename T>
struct Item {
  using first_type = T;
  using second_type = std::string_view;
  constexpr Item(first_type&& f, second_type&& s, source_location&& l = source_location::current())
    : first {std::forward<first_type>(f)}, second{std::forward<second_type>(s)}, loc{l} {}
  constexpr Item(const Item&) = default;
  constexpr Item(Item&&) = default;
  Item& operator=(const Item&) = default;
  Item& operator=(Item&&) = default;
  first_type first;
  second_type second;
  source_location loc;
};


inline auto make_bytes(string_view s) {
  vector<byte> result {};
  result.reserve(s.size());
  for(auto c : s) result.push_back(static_cast<byte>(c));
  return result;
}

template<size_t N>
auto make_byte_array(string_view s) {
  array<byte, N> result {};
  for(std::size_t i = 0; auto c : s) result[i++] = static_cast<byte>(c);
  return result;
}

template<typename Number>
auto test_name(const Number& number) {
  std::string type_name { meta::nameof<Number>() };
  type_name += "."s;
  type_name += tostr(number.v);
  return type_name;
}

using namespace boost::ut;
using namespace boost::ut::bdd;


inline constexpr char hex(std::uint8_t nibble) {
  static constexpr std::uint8_t c0 = '0';
  static constexpr std::uint8_t c7 = 'A' - 0xA;
  return static_cast<char>(nibble + (nibble > 9 ? c7:c0));
}

static_assert(hex(0x0) == '0');
static_assert(hex(0x9) == '9');
static_assert(hex(0xA) == 'A');
static_assert(hex(0xF) == 'F');

inline std::string to_hex(std::uint8_t c, char delim = 0) {
  if (delim != 0)
    return { delim, hex(c >> 4), hex(c & 0xF) };
  else
    return { hex(c >> 4), hex(c & 0xF) };
}

inline auto dump(std::string_view str) {
  std::string out {};
  out.reserve(str.size() * 3);
  char delim = 0;
  for(auto c : str) {
        out += to_hex(static_cast<std::uint8_t>(c), delim);
        delim = ' ';
  }
    return out;
}

enum class direction : std::uint32_t {
  none, serialize = 1, deserialize = 2, roundtrip = 4, all = 7
};

inline constexpr direction operator|(direction a, direction b) {
  return static_cast<direction>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline constexpr direction operator&(direction a, direction b) {
  return static_cast<direction>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

inline constexpr bool operator!(direction a) {
  return a == direction::none;
}

inline void add_test(const auto& tests, direction test_direction = direction::all) {
  using cute_type = std::remove_extent_t<std::remove_cvref_t<decltype(tests)>>::first_type;
  for(auto& item : tests) {
    if (!!(test_direction & direction::deserialize)) {
      test("des/" + test_name(item.first)) = [&item] {
        std::ispanstream in { item.second };
        cute_type actual {};
        const auto res = deserialize(in, actual);
        expect(res.errors() == parse_error::none);
        expect(neq(any(res), item.second.empty()), item.loc);
        expect(eq(actual, item.first), item.loc);
      };
    }
    if (!!(test_direction & direction::serialize)) {
      test("ser/" + test_name(item.first)) = [&item] {
        std::string actual(item.second.size(), '\000');
        std::ospanstream out { actual };
        serialize(out, item.first);
        expect(std::memcmp(actual.data(), item.second.data(), item.second.size()) == 0, item.loc)
                << "wire bytes mismatch: got\n" << dump(actual) << "\nexpected\n" << dump(item.second) << '\n';
      };
    }
    if (!!(test_direction & direction::roundtrip)) {
      test("rdt/" + test_name(item.first)) = [&item] {
        std::string buffer(item.second.size() * 2, '\000');
        std::ospanstream out { buffer };
        serialize(out, item.first);
        std::ispanstream in { buffer };
        cute_type actual {};
        const auto res = deserialize(in, actual);
        expect(res.errors() == parse_error::none);
        expect(neq(any(res), item.second.empty()), item.loc);
        expect(eq(actual, item.first), item.loc);
      };
    }
  }
}

inline void add_tests(const auto& ... tests) {
  (add_test(tests), ...);
}

inline void add_overflow_test(const auto& tests) {
    using cute_type = std::remove_extent_t<std::remove_cvref_t<decltype(tests)>>::first_type;
    for(auto& item : tests) {
        test("des/" + test_name(item.first)) = [&item] {
            std::ispanstream in { item.second };
            cute_type actual {};
            const auto res = deserialize(in, actual);
            expect(res.errors() == parse_error::integer_overflow, item.loc);
            expect(all(res), item.loc);
            expect(eq(actual, item.first), item.loc);
      };
  }
}

inline void add_overflow_tests(const auto& ... tests) {
  (add_overflow_test(tests), ...);
}

} // namespace testing
