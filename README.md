proto23
=======

A header-only C++ library for Protocol Buffers serialization and deserialization with fine-grained control over data types, no implicit dynamic memory allocation, and optional code generation.

It is a backport of proto26 [Practical C++26 reflection for Protobuf](https://hutorny.in.ua/projects/c/practical-c26-reflection-for-protobuf) to C++23.

## Features
- Header-only — drop into your project with no build steps
- No malloc required — fully compatible with embedded systems and safety-critical environments that ban dynamic allocation
- Explicit type control — choose exact integer widths, container types, and string representations
- Code-generation optional — define message structures directly in C++ without relying on protoc
- Standard streams interface — works with std::istream / std::ostream for easy integration
- Fixed-capacity buffers — inplacebuffer<N> provides stack-allocated, overflow-safe serialization
- Support for sub-sized integer types which enables serialization of legacy or external API structures

## Requirements

- C++23 or later
- C++ standard library

## Installation

- run `make install`
- or copy the `include/proto23` header directory into your include path. No linking required.

```cpp
#include <proto23/proto23.h>
#include <proto23/inplacebuffer.h>
```

## Quick Start

Define a message (no code generation)

```cpp
#include <proto23/proto23.h>
#include <cstdint>
#include <array>

struct SensorReading {
    uint32_t sensor_id;          // field 1: unsigned 32-bit
    int64_t  timestamp_us;       // field 2: signed 64-bit
    float    temperature;        // field 3: 32-bit float
    std::array<char, 32> label;  // field 4: fixed-size string buffer

    // proto23 field descriptors — explicit wire types, no reflection magic
    using Model = proto23::Fields<
      proto23:Field<&SensorReading::sensor_id, 1>,
      proto23:Field<&SensorReading::timestamp_us, 2>,
      proto23:Field<&SensorReading::temperature, 3>,
      proto23:Field<&SensorReading::label, 4>>;
};
```

Serialize to a fixed buffer (no heap)

```cpp
#include <array>
#include <spanstream>

std::array<char, 128> buf;
std::ospanstream out(buf);

SensorReading reading{42, 1715356800000000LL, 23.5f, {"main-hall"}};
proto23::serialize(out, reading);

if (out.good()) {
    // buf.data(), buf.size() — ready for transmission
    send(socket, out.span().data(), out.span().size());
}
```

## Type Control Examples

Using exact-width integers

```cpp
struct CompactEvent {
    uint8_t  event_type;   // uint32, subsized to fit 8-bit 
    int16_t  value;        // sint32, subsized to fit 16-bit 
    int32_t  delta;        // signed 32-bit
    prot23::fixed32 mask;  // fixed width unsigned 32-bit
    using Model = proto23::Fields<
      proto23:Field<&CompactEvent::event_type, 1>,
      proto23:Field<&CompactEvent::value, 2>,
      proto23:Field<&CompactEvent::delta, 3>;
      proto23:Field<&CompactEvent::mask, 4>>;
};
```

## Fixed-size arrays instead of std::vector

```cpp
struct Telemetry {
    std::array<float, 16> samples;  // exactly 16 floats, no heap
    std::array<uint8t, 6> macaddr;

    using Model = proto23::Fields<
      proto23:Field<&Telemetry::samples, 1>,
      proto23:Field<&Telemetry::macaddr, 2>>;
};
```

## Inplace [vectors](https://en.cppreference.com/cpp/container/inplace_vector) and inplace strings
```cpp
struct ADCReaidngs {
    inplace_vector<float, 8> samples;  // up to 8 floats, no heap
    inplace_string<64> raw_data;

    using Model = proto23::Fields<
      proto23:Field<&ADCReaidngs::samples, 1>,
      proto23:Field<&ADCReaidngs::raw_data, 2>>;
};
```

## Why Avoid Code Generation?

| Generated code (protoc)   | proto23 inline definitions  |
|---------------------------|-----------------------------|
| Requires build tooling    | Header-only, no extra steps |
| Types chosen by generator | You specify types and capacities |
| May pull in allocators    | Fully allocation-free capable |
| Schema changes need regeneration | Edit C++ directly    |

proto23 supports generated code when you want it—but doesn't require it.

## License

MIT — see LICENSE for details.
