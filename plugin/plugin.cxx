/*
 * Copyright (C) 2026 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * proto23/plugin/plugin.cxx - protoc plugin that generates C++23 structs
 * with proto23 Field<> model declarations from a .proto file.
 *
 * Protocol:
 *   stdin  <- google.protobuf.compiler.CodeGeneratorRequest  (binary proto)
 *   stdout -> google.protobuf.compiler.CodeGeneratorResponse (binary proto)
 *
 * Both messages are deserialised / serialised with the proto23 library
 * (no protobuf runtime required at build or run time of this plugin).
 *
 * Build:
 *   g++ -std=c++23 -Iinclude -o protoc-gen-proto23 plugin/plugin.cxx
 *
 * Usage:
 *   protoc --plugin=protoc-gen-proto23=./protoc-gen-proto23 \
 *          --proto23_out=. foo.proto
 *
 * Licensed under MIT License, see full text in LICENSE
 */

#include "descriptor.h"

#ifdef _WIN32
#  include <fcntl.h>
#  include <io.h>
#endif

#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// Type-name helpers
// ============================================================================

/// Strip the leading '.' and remove the package prefix from a fully-qualified
/// proto type name, then replace remaining '.' separators with '::'.
///
/// Examples (package = "example.pkg"):
///   ".example.pkg.Msg.Sub" -> "Msg::Sub"
///   ".google.protobuf.Any" -> "google::protobuf::Any"
static std::string resolve_name(const std::string& type_name,
                                const std::string& package)
{
    // Strip leading dot
    std::string fqn = (!type_name.empty() && type_name.front() == '.')
                      ? type_name.substr(1U)
                      : type_name;

    // Remove package prefix (e.g. "example.pkg.")
    if (!package.empty()) {
        const std::string prefix = package + '.';
        if (fqn.size() > prefix.size() &&
            fqn.compare(0U, prefix.size(), prefix) == 0) {
            fqn = fqn.substr(prefix.size());
        }
    }

    // Replace '.' with '::'
    std::string result;
    result.reserve(fqn.size() + 8U);
    for (const char c : fqn) {
        if (c == '.') { result += "::"; }
        else          { result += c;   }
    }
    return result;
}

/// Map a FieldDescriptorProto.Type integer to its C++ proto23 type string.
static std::string scalar_cpp_type(std::int32_t t)
{
    using F = pb::FieldDescriptorProto;
    switch (t) {
        case F::TYPE_BOOL:     return "bool";
        case F::TYPE_INT32:    return "proto::int32";
        case F::TYPE_SINT32:   return "proto::sint32";
        case F::TYPE_UINT32:   return "proto::uint32";
        case F::TYPE_FIXED32:  return "proto::fixed32";
        case F::TYPE_SFIXED32: return "proto::sfixed32";
        case F::TYPE_INT64:    return "proto::int64";
        case F::TYPE_SINT64:   return "proto::sint64";
        case F::TYPE_UINT64:   return "proto::uint64";
        case F::TYPE_FIXED64:  return "proto::fixed64";
        case F::TYPE_SFIXED64: return "proto::sfixed64";
        case F::TYPE_FLOAT:    return "float";
        case F::TYPE_DOUBLE:   return "double";
        case F::TYPE_STRING:   return "std::string";
        case F::TYPE_BYTES:    return "std::vector<std::byte>";
        default:               return "/* unknown_scalar_type */";
    }
}

/// Return the element type for a single-value occurrence of the field
/// (no container or optional wrapper).
static std::string element_type(const pb::FieldDescriptorProto& f,
                                const std::string& package)
{
    using F = pb::FieldDescriptorProto;
    const auto t = static_cast<std::int32_t>(f.type);
    if (t == F::TYPE_MESSAGE || t == F::TYPE_ENUM) {
        return resolve_name(f.type_name, package);
    }
    return scalar_cpp_type(t);
}

// ============================================================================
// Message registry (map<fqn -> DescriptorProto*>)
// ============================================================================

using Registry = std::map<std::string, const pb::DescriptorProto*>;

static void index_messages(const std::vector<pb::DescriptorProto>& msgs,
                           const std::string& prefix,
                           Registry& reg)
{
    for (const auto& m : msgs) {
        const std::string fqn = prefix + '.' + m.name;
        reg.emplace(fqn, &m);
        index_messages(m.nested_type, fqn, reg);
    }
}

static Registry build_registry(const pb::FileDescriptorProto& f)
{
    Registry reg;
    const std::string prefix =
        f.package.empty() ? std::string{} : ('.' + f.package);
    index_messages(f.message_type, prefix, reg);
    return reg;
}

static bool is_map_entry(const pb::DescriptorProto* msg) noexcept
{
    return msg != nullptr
        && msg->options.has_value()
        && msg->options->map_entry;
}

// ============================================================================
// Code generator
// ============================================================================

class Generator {
public:
    explicit Generator(const pb::FileDescriptorProto& f, const Registry& r)
        : file_{f}, reg_{r} {}

    std::string generate();

private:
    const pb::FileDescriptorProto& file_;
    const Registry&                reg_;

    // ---- Indentation ----
    static std::string ind(int n) {
        return std::string(static_cast<std::size_t>(n * 4), ' ');
    }

    // ---- Type resolution ----

    /// Full C++ type for a map field (std::map<K,V>).
    std::string map_type(const pb::DescriptorProto& entry) const
    {
        std::string kt, vt;
        for (const auto& ef : entry.field) {
            const auto fn = static_cast<std::int32_t>(ef.number);
            if (fn == 1)      kt = element_type(ef, file_.package);
            else if (fn == 2) vt = element_type(ef, file_.package);
        }
        return "std::map<" + kt + ", " + vt + ">";
    }

    /// C++ field declaration type (with container/optional/unique_ptr wrappers).
    std::string field_cpp_type(const pb::FieldDescriptorProto& f) const
    {
        using F = pb::FieldDescriptorProto;
        const auto label = static_cast<std::int32_t>(f.label);
        const auto type  = static_cast<std::int32_t>(f.type);
        const bool rep   = (label == F::LABEL_REPEATED);

        // Map field: repeated message with map_entry option
        if (rep && type == F::TYPE_MESSAGE) {
            auto it = reg_.find(f.type_name);
            if (it != reg_.end() && is_map_entry(it->second)) {
                return map_type(*it->second);
            }
        }

        if (rep) {
            // repeated bytes -> vector of blobs (each decoded as a separate LEN)
            if (type == F::TYPE_BYTES) {
                return "std::vector<std::vector<std::byte>>";
            }
            return "std::vector<" + element_type(f, file_.package) + ">";
        }

        // All non-repeated message fields -> std::unique_ptr<T>.
        // This handles both regular message fields and proto3-optional message
        // fields.  std::unique_ptr<T> is valid with a forward-declared (incomplete)
        // T, which is necessary because we forward-declare all top-level message
        // types at the top of the generated header.
        if (type == F::TYPE_MESSAGE) {
            return "std::unique_ptr<" + element_type(f, file_.package) + ">";
        }

        // proto3-optional scalar / enum -> std::optional<T>
        if (f.proto3_optional) {
            return "std::optional<" + element_type(f, file_.package) + ">";
        }

        return element_type(f, file_.package);
    }

    /// True if the message has at least one non-repeated, non-proto3-optional
    /// message-type field, which becomes a unique_ptr member requiring a
    /// user-declared (deferred) destructor.
    static bool needs_deferred_dtor(const pb::DescriptorProto& msg) noexcept
    {
        using F = pb::FieldDescriptorProto;
        for (const auto& f : msg.field) {
            // Both regular and proto3-optional message fields become unique_ptr<T>
            if (static_cast<std::int32_t>(f.type)  == F::TYPE_MESSAGE &&
                static_cast<std::int32_t>(f.label) != F::LABEL_REPEATED)
            {
                return true;
            }
        }
        return false;
    }

    // ---- Emitters ----

    void emit_enum(std::ostream& out,
                   const pb::EnumDescriptorProto& e,
                   int depth) const
    {
        out << ind(depth) << "enum class " << e.name << " : std::int32_t {\n";
        for (const auto& v : e.value) {
            out << ind(depth + 1)
                << v.name << " = "
                << static_cast<std::int32_t>(v.number) << ",\n";
        }
        out << ind(depth) << "};\n\n";
    }

    /// Emit a struct for one DescriptorProto.
    ///
    /// @param qual_prefix  fully-qualified C++ prefix for the destructor
    ///                     definition, e.g. "" for top-level or "Outer::"
    ///                     for a nested struct.
    /// @param deferred_dtors  accumulated inline destructor definitions to
    ///                        be emitted after all types are complete.
    void emit_message(std::ostream&                 out,
                      const pb::DescriptorProto&    msg,
                      const std::string&            qual_prefix,
                      int                           depth,
                      std::vector<std::string>&     deferred_dtors) const
    {
        // Auto-generated map-entry messages are not emitted directly;
        // they are represented as std::map<K,V> fields in the parent.
        if (is_map_entry(&msg)) { return; }

        const bool need_dtor = needs_deferred_dtor(msg);

        out << ind(depth) << "struct " << msg.name << " {\n";

        // Nested enums
        for (const auto& e : msg.enum_type) {
            emit_enum(out, e, depth + 1);
        }

        // Nested messages (recursive).  Their deferred-dtor entries are
        // collected into the SAME deferred_dtors vector so that they are
        // all emitted at namespace scope after every type is complete.
        const std::string child_prefix = qual_prefix + msg.name + "::";
        for (const auto& nm : msg.nested_type) {
            emit_message(out, nm, child_prefix, depth + 1, deferred_dtors);
        }

        // Build oneof groups: oneof_index -> ordered field list
        // (skip proto3-optional synthetic oneofs)
        std::map<std::int32_t,
                 std::vector<const pb::FieldDescriptorProto*>> oneof_groups;
        for (const auto& f : msg.field) {
            if (f.oneof_index.has_value() && !f.proto3_optional) {
                oneof_groups[static_cast<std::int32_t>(*f.oneof_index)].push_back(&f);
            }
        }

        // Field declarations — iterate in proto order
        std::set<std::int32_t> emitted;
        for (const auto& f : msg.field) {
            if (f.oneof_index.has_value() && !f.proto3_optional) {
                // Real oneof: emit as std::variant on first encounter
                const auto oidx = static_cast<std::int32_t>(*f.oneof_index);
                if (emitted.count(oidx) != 0U) { continue; }
                emitted.insert(oidx);

                const auto& oname =
                    msg.oneof_decl.at(static_cast<std::size_t>(oidx)).name;
                const auto& ofields = oneof_groups.at(oidx);

                out << ind(depth + 1) << "std::variant<";
                for (std::size_t i = 0U; i < ofields.size(); ++i) {
                    if (i != 0U) { out << ", "; }
                    out << element_type(*ofields[i], file_.package);
                }
                out << "> " << oname << "{};\n";
            } else {
                out << ind(depth + 1) << field_cpp_type(f) << " " << f.name << "{};\n";
            }
        }

        // Explicit special-member declarations for unique_ptr fields
        if (need_dtor) {
            out << "\n";
            out << ind(depth + 1) << msg.name << "() = default;\n";
            out << ind(depth + 1) << msg.name << "(" << msg.name << "&&) = default;\n";
            out << ind(depth + 1) << msg.name
                                  << "& operator=(" << msg.name << "&&) = default;\n";
            out << ind(depth + 1) << "~" << msg.name << "();\n";

            deferred_dtors.push_back(
                "inline " + qual_prefix + msg.name
                + "::~" + msg.name + "() = default;");
        }

        // using Model = proto::Fields<...>
        out << "\n" << ind(depth + 1) << "using Model = proto::Fields<\n";

        emitted.clear();
        bool first = true;

        auto comma = [&]() {
            if (!first) { out << ",\n"; }
            first = false;
        };

        for (const auto& f : msg.field) {
            if (f.oneof_index.has_value() && !f.proto3_optional) {
                // OneOf entry on first encounter
                const auto oidx = static_cast<std::int32_t>(*f.oneof_index);
                if (emitted.count(oidx) != 0U) { continue; }
                emitted.insert(oidx);

                const auto& oname =
                    msg.oneof_decl.at(static_cast<std::size_t>(oidx)).name;
                const auto& ofields = oneof_groups.at(oidx);

                comma();
                out << ind(depth + 2)
                    << "proto::OneOf<&" << msg.name << "::" << oname;
                for (const auto* of : ofields) {
                    out << ", " << static_cast<std::int32_t>(of->number);
                }
                out << ">";
            } else {
                comma();
                out << ind(depth + 2)
                    << "proto::Field<&" << msg.name << "::" << f.name
                    << ", " << static_cast<std::int32_t>(f.number) << ">";
            }
        }
        out << ">;\n";
        out << ind(depth) << "};\n\n";
    }

    /// Convert "a.b.c" package string to "a::b::c" C++ namespace.
    static std::string pkg_to_ns(const std::string& pkg)
    {
        std::string ns;
        ns.reserve(pkg.size() + 4U);
        for (const char c : pkg) {
            if (c == '.') { ns += "::"; }
            else          { ns += c; }
        }
        return ns;
    }
};

// ---- Generator::generate() ----

std::string Generator::generate()
{
    std::ostringstream out;
    const auto& pkg = file_.package;

    // --- File header ---
    out << "// Generated by protoc-gen-proto23 -- DO NOT EDIT\n"
        << "// Source: " << file_.name << "\n\n"
        << "#pragma once\n\n"
        << "#include <proto/proto.h>\n\n"
        << "#include <cstddef>\n"
        << "#include <cstdint>\n"
        << "#include <map>\n"
        << "#include <memory>\n"
        << "#include <optional>\n"
        << "#include <string>\n"
        << "#include <variant>\n"
        << "#include <vector>\n\n";

    // Dependency includes (replace .proto with .proto23.h)
    for (const auto& dep : file_.dependency) {
        const auto dot = dep.rfind(".proto");
        const std::string base =
            (dot != std::string::npos) ? dep.substr(0U, dot) : dep;
        out << "#include \"" << base << ".proto23.h\"\n";
    }
    if (!file_.dependency.empty()) { out << "\n"; }

    // --- Namespace open ---
    const std::string ns = pkg.empty() ? std::string{} : pkg_to_ns(pkg);
    if (!ns.empty()) { out << "namespace " << ns << " {\n\n"; }

    // Forward-declare all top-level message types so that unique_ptr<T>
    // members can reference types defined further down in the file.
    for (const auto& m : file_.message_type) {
        if (!is_map_entry(&m)) {
            out << "struct " << m.name << ";\n";
        }
    }
    if (!file_.message_type.empty()) { out << "\n"; }

    // File-level enum definitions
    for (const auto& e : file_.enum_type) {
        emit_enum(out, e, 0);
    }

    // Message struct definitions
    std::vector<std::string> deferred_dtors;
    for (const auto& m : file_.message_type) {
        emit_message(out, m, /*qual_prefix=*/"", /*depth=*/0, deferred_dtors);
    }

    // Deferred destructor definitions (all types are complete here)
    if (!deferred_dtors.empty()) {
        out << "// Destructor definitions — all types complete at this point.\n";
        for (const auto& dd : deferred_dtors) {
            out << dd << "\n";
        }
        out << "\n";
    }

    // --- Namespace close ---
    if (!ns.empty()) { out << "} // namespace " << ns << "\n"; }

    return out.str();
}

// ============================================================================
// Output filename
// ============================================================================

static std::string output_filename(const std::string& proto_name)
{
    // "foo/bar.proto" -> "foo/bar.proto23.h"
    const auto dot = proto_name.rfind(".proto");
    if (dot != std::string::npos) {
        return proto_name.substr(0U, dot) + ".proto23.h";
    }
    return proto_name + ".proto23.h";
}

// ============================================================================
// main
// ============================================================================

int main()
{
#ifdef _WIN32
    // Switch stdin/stdout to binary mode to avoid CRLF translation
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    // Read the entire CodeGeneratorRequest from stdin into a string
    const std::string raw{
        std::istreambuf_iterator<char>(std::cin),
        std::istreambuf_iterator<char>{}};

    std::istringstream iss{raw};
    pb::CodeGeneratorRequest request{};
    (void)proto23::deserialize(iss, request);

    // Index proto_file by name for quick lookup
    std::map<std::string, const pb::FileDescriptorProto*> file_map;
    for (const auto& f : request.proto_file) {
        file_map.emplace(f.name, &f);
    }

    pb::CodeGeneratorResponse response{};
    response.supported_features = pb::CodeGeneratorResponse::FEATURE_PROTO3_OPTIONAL;

    for (const auto& fname : request.file_to_generate) {
        const auto it = file_map.find(fname);
        if (it == file_map.end()) {
            response.error = "requested file not found in proto_file list: " + fname;
            break;
        }
        const auto& fdp = *it->second;

        const Registry     reg = build_registry(fdp);
        Generator          gen{fdp, reg};

        pb::CodeGeneratorResponseFile outf{};
        outf.name    = output_filename(fname);
        outf.content = gen.generate();
        response.file.push_back(std::move(outf));
    }

    // Serialise the response and write it to stdout
    std::ostringstream oss;
    proto23::serialize(oss, response);
    const auto output = oss.str();
    std::cout.write(output.data(), static_cast<std::streamsize>(output.size()));
    std::cout.flush();

    return 0;
}
