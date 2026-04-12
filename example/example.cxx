/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * proto23/example/example.cxx - Usage examples for the C++23 proto library.
 *
 * Demonstrates:
 *  1. Basic scalar message
 *  2. Nested messages
 *  3. Repeated fields (vectors)
 *  4. Optional and unique_ptr fields
 *  5. Oneof (std::variant)
 *  6. Map fields
 *  7. Custom wire types (proto23::sint32, proto23::fixed64, ...)
 *
 * Licensed under MIT License, see full text in LICENSE
 */

#include <proto23/proto23.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

// ==========================================================================
// Example 1: Basic scalar message
// ==========================================================================

struct Address {
    std::string         city{};
    std::string         street{};
    std::uint32_t       zip_code{};

    using Model = proto23::Fields<
        proto23::Field<&Address::city,     1>,
        proto23::Field<&Address::street,   2>,
        proto23::Field<&Address::zip_code, 3>>;
};

// ==========================================================================
// Example 2: Nested message + optional + unique_ptr
// ==========================================================================

struct Person {
    std::string                  name{};
    std::int32_t                 age{};          // zigzag (native int32_t)
    std::optional<Address>       home{};
    std::unique_ptr<Person>      assistant{};

    using Model = proto23::Fields<
        proto23::Field<&Person::name,      1>,
        proto23::Field<&Person::age,       2>,
        proto23::Field<&Person::home,      3>,
        proto23::Field<&Person::assistant, 4>>;
};

// ==========================================================================
// Example 3: Repeated fields
// ==========================================================================

struct PhoneBook {
    std::string              owner{};
    std::vector<std::string> numbers{};         // unpacked (strings)
    std::vector<std::int32_t> scores{};         // packed (zigzag ints)

    using Model = proto23::Fields<
        proto23::Field<&PhoneBook::owner,   1>,
        proto23::Field<&PhoneBook::numbers, 2>,
        proto23::Field<&PhoneBook::scores,  3>>;
};

// ==========================================================================
// Example 4: Oneof (std::variant)
//   Field 10 -> int alternative, field 11 -> double, field 12 -> string
// ==========================================================================

struct Measurement {
    std::string                           sensor{};
    std::variant<int, double, std::string> reading{};

    using Model = proto23::Fields<
        proto23::Field<&Measurement::sensor,  1>,
        proto23::OneOf<&Measurement::reading, 10, 11, 12>>;
};

// ==========================================================================
// Example 5: Map field
// ==========================================================================

struct Config {
    std::string                        name{};
    std::map<std::string, std::string> params{};

    using Model = proto23::Fields<
        proto23::Field<&Config::name,   1>,
        proto23::Field<&Config::params, 2>>;
};

// ==========================================================================
// Example 6: Custom protobuf wire types
// ==========================================================================

struct Coordinates {
    proto23::sfixed64 x{};   // 64-bit signed, fixed encoding
    proto23::sfixed64 y{};
    proto23::sfixed32 floor{};
    float           elevation{};

    using Model = proto23::Fields<
        proto23::Field<&Coordinates::x,         1>,
        proto23::Field<&Coordinates::y,         2>,
        proto23::Field<&Coordinates::floor,     3>,
        proto23::Field<&Coordinates::elevation, 4>>;
};

// ==========================================================================
// Example 7: Result tracking — a two-field message for partial deserialisation
// ==========================================================================

enum class Status : std::int32_t { NOTOK = 0, OK = 42 };

struct Inner {
    Status       status{};
    std::int32_t v{};

    using Model = proto23::Fields<
        proto23::Field<&Inner::status, 1>,
        proto23::Field<&Inner::v,      2>>;
};

// ==========================================================================
// Helpers
// ==========================================================================

template<proto23::message Msg>
Msg round_trip(const Msg& original) {
    std::ostringstream out;
    proto23::serialize(out, original);
    const auto wire = out.str();
    std::istringstream in{wire};
    Msg result{};
    const auto res = proto23::deserialize(in, result);
    // In a real application, check that expected fields are present:
    // assert(proto23::all(res));
    (void)res;
    return result;
}

template<proto23::message Msg>
void print_size(const char* name, const Msg& msg) {
    std::ostringstream out;
    proto23::serialize(out, msg);
    std::cout << name << ": " << out.str().size() << " bytes\n";
}

// ==========================================================================
// main
// ==========================================================================

int main() {
    // -----------------------------------------------------------------------
    // 1. Basic scalar round-trip
    // -----------------------------------------------------------------------
    {
        Address a{"Berlin", "Unter den Linden 77", 10117U};
        const auto rt = round_trip(a);
        assert(rt.city     == a.city);
        assert(rt.street   == a.street);
        assert(rt.zip_code == a.zip_code);
        print_size("Address", a);
    }

    // -----------------------------------------------------------------------
    // 2. Nested + optional + unique_ptr
    // -----------------------------------------------------------------------
    {
        Person p;
        p.name      = "Alice";
        p.age       = 30;
        p.home      = Address{"London", "Baker St 221B", 0U};
        p.assistant = std::make_unique<Person>();
        p.assistant->name = "Bob";
        p.assistant->age  = 25;

        const auto rt = round_trip(p);
        assert(rt.name == p.name);
        assert(rt.age  == p.age);
        assert(rt.home.has_value());
        assert(rt.home->city == "London");
        assert(rt.assistant != nullptr);
        assert(rt.assistant->name == "Bob");
        print_size("Person", p);
    }

    // -----------------------------------------------------------------------
    // 3. Repeated fields
    // -----------------------------------------------------------------------
    {
        PhoneBook pb;
        pb.owner   = "Charlie";
        pb.numbers = {"+1-555-1234", "+44-20-1234-5678"};
        pb.scores  = {10, -5, 42, -99};

        const auto rt = round_trip(pb);
        assert(rt.owner   == pb.owner);
        assert(rt.numbers == pb.numbers);
        assert(rt.scores  == pb.scores);
        print_size("PhoneBook", pb);
    }

    // -----------------------------------------------------------------------
    // 4. Oneof / variant
    // -----------------------------------------------------------------------
    {
        Measurement m1;
        m1.sensor  = "temperature";
        m1.reading = 36.6;           // double -> field 11

        Measurement m2;
        m2.sensor  = "level";
        m2.reading = 42;             // int -> field 10

        Measurement m3;
        m3.sensor  = "label";
        m3.reading = std::string{"critical"};  // string -> field 12

        for (const auto* mp : {&m1, &m2, &m3}) {
            const auto rt = round_trip(*mp);
            assert(rt.sensor  == mp->sensor);
            assert(rt.reading == mp->reading);
        }
        print_size("Measurement (double)", m1);
    }

    // -----------------------------------------------------------------------
    // 5. Map
    // -----------------------------------------------------------------------
    {
        Config cfg;
        cfg.name   = "server";
        cfg.params = {{"host", "localhost"}, {"port", "8080"}, {"debug", "true"}};

        const auto rt = round_trip(cfg);
        assert(rt.name   == cfg.name);
        assert(rt.params == cfg.params);
        print_size("Config", cfg);
    }

    // -----------------------------------------------------------------------
    // 6. Custom wire types (sfixed64, sfixed32, float)
    // -----------------------------------------------------------------------
    {
        Coordinates c{
            proto23::sfixed64{123456789LL},
            proto23::sfixed64{-987654321LL},
            proto23::sfixed32{5},
            48.8566F
        };

        const auto rt = round_trip(c);
        assert(static_cast<std::int64_t>(rt.x) == static_cast<std::int64_t>(c.x));
        assert(static_cast<std::int64_t>(rt.y) == static_cast<std::int64_t>(c.y));
        assert(static_cast<std::int32_t>(rt.floor) == static_cast<std::int32_t>(c.floor));
        assert(rt.elevation == c.elevation);
        print_size("Coordinates", c);
    }

    // -----------------------------------------------------------------------
    // 7. Result tracking: partial message — only field 1 present in wire
    // -----------------------------------------------------------------------
    {
        const std::string wire = "\x08\x2A";   // field 1 = varint 42 (Status::OK)
        std::istringstream in{wire};
        Inner msg{};
        const auto result = proto23::deserialize(in, msg);

        assert( proto23::any(result));         // at least one field found
        assert(!proto23::all(result));         // not all fields found (v absent)
        assert(static_cast<std::int32_t>(msg.status) == 42);
        assert(msg.v == 0);                  // field 2 was absent
    }

    std::cout << "All examples passed.\n";
    return 0;
}
