/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * proto23/plugin/descriptor.h - Minimal C++23/proto23 mirrors of the
 * google.protobuf descriptor and google.protobuf.compiler.plugin types.
 *
 * Only the fields required by protoc-gen-proto23 are mapped.  The proto23
 * library is used for deserialization — no protobuf runtime dependency.
 *
 * Licensed under MIT License, see full text in LICENSE
 */
#pragma once

#include <proto23/proto23.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace proto23::options {
enum class Inplace {
    UNSPECIFIED = 0,
    YES = 1,
    NO = 2,
};

} // namespace proto23::options

namespace pb {

// ---------------------------------------------------------------------------
// google.protobuf.EnumValueDescriptorProto  (descriptor.proto)
// ---------------------------------------------------------------------------
struct EnumValueDescriptorProto {
    std::string  name{};
    proto23::int32 number{};

    using Model = proto23::Fields<
        proto23::Field<&EnumValueDescriptorProto::name,   1>,
        proto23::Field<&EnumValueDescriptorProto::number, 2>>;
};

// ---------------------------------------------------------------------------
// google.protobuf.EnumDescriptorProto
// ---------------------------------------------------------------------------
struct EnumDescriptorProto {
    std::string                           name{};
    std::vector<EnumValueDescriptorProto> value{};

    using Model = proto23::Fields<
        proto23::Field<&EnumDescriptorProto::name,  1>,
        proto23::Field<&EnumDescriptorProto::value, 2>>;
};

// ---------------------------------------------------------------------------
// google.protobuf.OneofDescriptorProto
// ---------------------------------------------------------------------------
struct OneofDescriptorProto {
    std::string name{};

    using Model = proto23::Fields<
        proto23::Field<&OneofDescriptorProto::name, 1>>;
};

// ---------------------------------------------------------------------------
// google.protobuf.FieldOptions (subset)
// ---------------------------------------------------------------------------
struct FieldOptions {
    proto23::options::Inplace inplace{};                     // field 68000
    std::uint32_t capacity{};                                // field 68001
    bool packed{};                                           // field 2
    bool deprecated{};                                       // field 3
    using Model = proto23::Fields<
        proto23::Field<&FieldOptions::packed, 2>,
        proto23::Field<&FieldOptions::deprecated, 3>,
        proto23::Field<&FieldOptions::inplace, 68000>,
        proto23::Field<&FieldOptions::capacity, 68001>>;
};

// ---------------------------------------------------------------------------
// google.protobuf.FieldDescriptorProto  (subset)
//
// oneof_index uses std::optional<proto23::int32> so that the value 0 (first
// oneof) can be distinguished from "absent" (field is not in any oneof).
// The descriptor.proto uses proto2 optional semantics, so value 0 IS
// serialised on the wire.
// ---------------------------------------------------------------------------
struct FieldDescriptorProto {
    std::string                   name{};             // field  1
    std::string                   extendee{};         // field  2  (ignored)
    proto23::int32                number{};           // field  3
    proto23::int32                label{};            // field  4  Label enum
    proto23::int32                type{};             // field  5  Type enum
    std::string                   type_name{};        // field  6  for MSG/ENUM
    std::optional<FieldOptions>   options{};          // field  7
    std::optional<proto23::int32> oneof_index{};      // field  9  absent ⇒ not in oneof
    bool                          proto3_optional{};  // field 17

    // FieldDescriptorProto.Type enum values
    static constexpr std::int32_t TYPE_DOUBLE   =  1;
    static constexpr std::int32_t TYPE_FLOAT    =  2;
    static constexpr std::int32_t TYPE_INT64    =  3;
    static constexpr std::int32_t TYPE_UINT64   =  4;
    static constexpr std::int32_t TYPE_INT32    =  5;
    static constexpr std::int32_t TYPE_FIXED64  =  6;
    static constexpr std::int32_t TYPE_FIXED32  =  7;
    static constexpr std::int32_t TYPE_BOOL     =  8;
    static constexpr std::int32_t TYPE_STRING   =  9;
    static constexpr std::int32_t TYPE_GROUP    = 10;
    static constexpr std::int32_t TYPE_MESSAGE  = 11;
    static constexpr std::int32_t TYPE_BYTES    = 12;
    static constexpr std::int32_t TYPE_UINT32   = 13;
    static constexpr std::int32_t TYPE_ENUM     = 14;
    static constexpr std::int32_t TYPE_SFIXED32 = 15;
    static constexpr std::int32_t TYPE_SFIXED64 = 16;
    static constexpr std::int32_t TYPE_SINT32   = 17;
    static constexpr std::int32_t TYPE_SINT64   = 18;

    // FieldDescriptorProto.Label enum values
    static constexpr std::int32_t LABEL_OPTIONAL = 1;
    static constexpr std::int32_t LABEL_REQUIRED = 2;
    static constexpr std::int32_t LABEL_REPEATED = 3;

    using Model = proto23::Fields<
        proto23::Field<&FieldDescriptorProto::name,            1>,
        proto23::Field<&FieldDescriptorProto::extendee,        2>,
        proto23::Field<&FieldDescriptorProto::number,          3>,
        proto23::Field<&FieldDescriptorProto::label,           4>,
        proto23::Field<&FieldDescriptorProto::type,            5>,
        proto23::Field<&FieldDescriptorProto::type_name,       6>,
        proto23::Field<&FieldDescriptorProto::options,         8>,
        proto23::Field<&FieldDescriptorProto::oneof_index,     9>,
        proto23::Field<&FieldDescriptorProto::proto3_optional, 17>>;
};

// ---------------------------------------------------------------------------
// google.protobuf.MessageOptions  (subset — only map_entry needed)
// ---------------------------------------------------------------------------
struct MessageOptions {
    proto23::int32 message_id{};
    bool deprecated{};              // field 3
    bool map_entry{};              // field 7

    using Model = proto23::Fields<
        proto23::Field<&MessageOptions::deprecated, 3>,
        proto23::Field<&MessageOptions::map_entry, 7>,
        proto23::Field<&MessageOptions::message_id, 68228>>;
};

// ---------------------------------------------------------------------------
// google.protobuf.DescriptorProto  (subset)
//
// nested_type is self-referential.  Per C++17 [vector.overview] (and C++23)
// std::vector<T> may be instantiated with an incomplete T provided no member
// function is called until T is complete; this is satisfied here because all
// member functions are instantiated only in plugin.cxx where DescriptorProto
// is fully defined.
// ---------------------------------------------------------------------------
struct DescriptorProto {
    std::string                       name{};
    std::vector<FieldDescriptorProto> field{};
    std::vector<DescriptorProto>      nested_type{};   // <- recursive, ok in C++17+
    std::vector<EnumDescriptorProto>  enum_type{};
    std::vector<OneofDescriptorProto> oneof_decl{};
    std::optional<MessageOptions>     options{};

    using Model = proto23::Fields<
        proto23::Field<&DescriptorProto::name,        1>,
        proto23::Field<&DescriptorProto::field,       2>,
        proto23::Field<&DescriptorProto::nested_type, 3>,
        proto23::Field<&DescriptorProto::enum_type,   4>,
        proto23::Field<&DescriptorProto::oneof_decl,  8>,
        proto23::Field<&DescriptorProto::options,     7>>;

    mutable const DescriptorProto* parent {};  // Internal use only
};

// ---------------------------------------------------------------------------
// google.protobuf.FileOptions  (subset)
// ---------------------------------------------------------------------------
struct FileOptions {
    proto23::options::Inplace inplace_string{};   // field 68000
    std::uint32_t default_string_capacity{};      // field 68001;
    proto23::options::Inplace inplace_repeated{}; // field 68002
    std::uint32_t default_repeated_capacity{};    // field 68003;
    proto23::options::Inplace inplace_message{};  // field 68004
    bool deprecated{};                            // field 23
    using Model = proto23::Fields<
        proto23::Field<&FileOptions::deprecated, 23>,
        proto23::Field<&FileOptions::inplace_string, 68000>,
        proto23::Field<&FileOptions::default_string_capacity, 68001>,
        proto23::Field<&FileOptions::inplace_repeated, 68002>,
        proto23::Field<&FileOptions::default_repeated_capacity, 68003>,
        proto23::Field<&FileOptions::inplace_message, 68004>>;
};

// ---------------------------------------------------------------------------
// google.protobuf.FileDescriptorProto  (subset)
// ---------------------------------------------------------------------------
struct FileDescriptorProto {
    std::string                      name{};
    std::string                      package{};
    std::vector<std::string>         dependency{};
    std::vector<DescriptorProto>     message_type{};
    std::vector<EnumDescriptorProto> enum_type{};
    std::optional<FileOptions>       options{};
    std::string                      syntax{};

    using Model = proto23::Fields<
        proto23::Field<&FileDescriptorProto::name,         1>,
        proto23::Field<&FileDescriptorProto::package,      2>,
        proto23::Field<&FileDescriptorProto::dependency,   3>,
        proto23::Field<&FileDescriptorProto::message_type, 4>,
        proto23::Field<&FileDescriptorProto::enum_type,    5>,
        proto23::Field<&FileDescriptorProto::options,      8>,
        proto23::Field<&FileDescriptorProto::syntax,      12>>;
};

// ---------------------------------------------------------------------------
// google.protobuf.compiler.CodeGeneratorRequest  (plugin.proto)
// ---------------------------------------------------------------------------
struct CodeGeneratorRequest {
    std::vector<std::string>         file_to_generate{};
    std::string                      parameter{};
    std::vector<FileDescriptorProto> proto_file{};

    using Model = proto23::Fields<
        proto23::Field<&CodeGeneratorRequest::file_to_generate, 1>,
        proto23::Field<&CodeGeneratorRequest::parameter,        2>,
        proto23::Field<&CodeGeneratorRequest::proto_file,      15>>;
};

// ---------------------------------------------------------------------------
// google.protobuf.compiler.CodeGeneratorResponse.File
// ---------------------------------------------------------------------------
struct CodeGeneratorResponseFile {
    std::string name{};
    std::string insertion_point{};
    std::string content{};

    using Model = proto23::Fields<
        proto23::Field<&CodeGeneratorResponseFile::name,            1>,
        proto23::Field<&CodeGeneratorResponseFile::insertion_point, 2>,
        proto23::Field<&CodeGeneratorResponseFile::content,        15>>;
};

// ---------------------------------------------------------------------------
// google.protobuf.compiler.CodeGeneratorResponse
// ---------------------------------------------------------------------------
struct CodeGeneratorResponse {
    std::string                           error{};
    std::uint64_t                         supported_features{};
    std::vector<CodeGeneratorResponseFile> file{};

    static constexpr std::uint64_t FEATURE_PROTO3_OPTIONAL   = 1U;
    static constexpr std::uint64_t FEATURE_SUPPORTS_EDITIONS = 2U;

    using Model = proto23::Fields<
        proto23::Field<&CodeGeneratorResponse::error,              1>,
        proto23::Field<&CodeGeneratorResponse::supported_features, 2>,
        proto23::Field<&CodeGeneratorResponse::file,              15>>;
};

} // namespace pb
