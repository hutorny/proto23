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
#include <proto23/diagnostic.h>

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
#include <unordered_map>
#include <unordered_set>

static constexpr std::initializer_list<std::string_view> cplusplus_keywords {
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char",
    "char8_t", "char16_t", "char32_t", "class", "compl", "concept", "const", "const_cast", "consteval", "constexpr",
    "constinit", "continue", "co_await", "co_return", "co_yield", "decltype", "default", "delete", "do", "double",
    "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float", "for", "friend", "goto", "if",
    "inline", "int", "long", "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or",
    "or_eq", "private", "protected", "public", "register", "reinterpret_cast", "requires", "return", "short", "signed",
    "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "template", "this", "thread_local", "throw",
    "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile",
    "wchar_t", "while", "xor", "xor_eq", "NULL" };

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
                                const std::string& package) {
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
    std::string result{};
    result.reserve(fqn.size() + 8U);
    for (const char c : fqn) {
        if (c == '.') { result += "::"; }
        else          { result += c;   }
    }
    return result;
}

/// Map a FieldDescriptorProto.Type integer to its C++ proto23 type string.
static std::string scalar_cpp_type(std::int32_t t) {
    using F = pb::FieldDescriptorProto;
    switch (t) {
        case F::TYPE_BOOL:     return "bool";
        case F::TYPE_INT32:    return "proto23::int32";
        case F::TYPE_SINT32:   return "std::int32_t";
        case F::TYPE_UINT32:   return "std::uint32_t";
        case F::TYPE_FIXED32:  return "proto23::fixed32";
        case F::TYPE_SFIXED32: return "proto23::sfixed32";
        case F::TYPE_INT64:    return "proto23::int64";
        case F::TYPE_SINT64:   return "std::int64_t";
        case F::TYPE_UINT64:   return "std::uint64_t";
        case F::TYPE_FIXED64:  return "proto23::fixed64";
        case F::TYPE_SFIXED64: return "proto23::sfixed64";
        case F::TYPE_FLOAT:    return "float";
        case F::TYPE_DOUBLE:   return "double";
        case F::TYPE_STRING:   return "std::string";
        case F::TYPE_BYTES:    return "std::vector<std::byte>";
        default:               return "/* unknown_scalar_type */";
    }
}


static std::string_view packed_attr(bool rep, const std::optional<pb::FieldOptions>& options) {
  return (rep && options && !options->packed) ? std::string_view{", proto23::packed_t::no"} : std::string_view{};
}

/// Return the element type for a single-value occurrence of the field
/// (no container or optional wrapper).
static std::string_view sanitizing_suffix(const std::string& type_name) {
    static const std::unordered_set<std::string_view> keywords {cplusplus_keywords};
    return keywords.contains(type_name) ? std::string_view{"_"} : std::string_view{};
}

/// Return the element type for a single-value occurrence of the field
/// (no container or optional wrapper).
static std::string element_type(const pb::FieldDescriptorProto& f,
                                const std::string& package) {
    using F = pb::FieldDescriptorProto;
    const auto t = static_cast<std::int32_t>(f.type);
    if (t == F::TYPE_MESSAGE || t == F::TYPE_ENUM) {
        return resolve_name(f.type_name + std::string{sanitizing_suffix(f.type_name)}, package);
    }
    return scalar_cpp_type(t);
}

// ============================================================================
// Message registry (map<fqn -> DescriptorProto*>)
// ============================================================================
using Registry = std::map<std::string, const pb::DescriptorProto*>;

static void index_messages(const std::vector<pb::DescriptorProto>& msgs,
                           const std::string& prefix,
                           Registry& reg, const pb::DescriptorProto* parent = nullptr) {
    for (const auto& m : msgs) {
        const std::string fqn = prefix + '.' + m.name;
        reg.emplace(fqn, &m);
        m.parent = parent;
        index_messages(m.nested_type, fqn, reg, &m);
    }
}

static Registry build_registry(const pb::FileDescriptorProto& f) {
    Registry reg{};
    const std::string prefix =
        f.package.empty() ? std::string{} : ('.' + f.package);
    index_messages(f.message_type, prefix, reg);
    return reg;
}

static bool is_map_entry(const pb::DescriptorProto* msg) noexcept {
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
    using F = pb::FieldDescriptorProto;
    struct traits {
      bool complete;
      bool noinit;
    };
    static constexpr pb::FileOptions default_options = {
        .inplace_string = {}, .default_string_capacity = 64,
        .inplace_repeated = {}, .default_repeated_capacity = 16,
        .inplace_message = {}, .deprecated = {}};
    const pb::FileDescriptorProto& file_;
    const Registry&                reg_;
    pb::FileOptions                options_ { default_options };
    std::unordered_map<std::string, traits> complete_ {};

    traits is_complete(const std::string& fqn) const noexcept {
      const auto found = complete_.find(fqn);
      if (found == complete_.end()) return {false, true};
      return found->second;
    }
    void complete(const std::string& fqn, bool noinit) noexcept {
      complete_[fqn] = traits{true, noinit};
    }
    // ---- Indentation ----
    static std::string ind(int n) {
        return std::string(static_cast<std::size_t>(n * 4), ' ');
    }

    // ---- Type resolution ----

    /// Full C++ type for a map field (std::map<K,V>).
    std::string map_type(const pb::DescriptorProto& entry) const {
        std::string kt{}, vt{};
        for (const auto& ef : entry.field) {
            const auto fn = static_cast<std::int32_t>(ef.number);
            if (fn == 1)      kt = element_type(ef, file_.package);
            else if (fn == 2) vt = element_type(ef, file_.package);
        }
        return "std::map<" + kt + ", " + vt + ">";
    }
    struct CppType {
      std::string name;
      bool noinit;
    };
    /// Derive field options from file options
    static pb::FieldOptions make_options(int type, bool repeated, pb::FieldOptions field_options, pb::FileOptions file_options) {
        using proto23::options::Inplace;
        pb::FieldOptions result {field_options};
        if (repeated) {
          if (field_options.inplace == Inplace::UNSPECIFIED) {
            result.inplace = file_options.inplace_repeated;
          }
          if (field_options.capacity == 0) {
            result.capacity = file_options.default_repeated_capacity;
          }
          return result;
        }
        switch(type) {
        case F::TYPE_MESSAGE:
            if (field_options.inplace == Inplace::UNSPECIFIED) {
              result.inplace = file_options.inplace_message;
            }
            break;
        case F::TYPE_STRING:
        case F::TYPE_BYTES:
            if (field_options.inplace == Inplace::UNSPECIFIED) {
              result.inplace = file_options.inplace_string;
            }
            if (field_options.capacity == 0) {
              result.capacity = file_options.default_string_capacity;
            }
            break;
        default:
           break;
        }
        return result;
    }
    /// returns true if msg is an inner type of parent
    bool is_inner(const pb::DescriptorProto* msg, const pb::DescriptorProto* parent) const {
      if (msg == nullptr) return false;
      unsigned guard = 200;
      while(parent != nullptr && --guard != 0) {
        if (parent == msg->parent) return true;
        parent = parent->parent;
      }
      return false;
    }
    /// C++ field declaration type (with container/optional/unique_ptr wrappers).
    CppType field_cpp_type(const pb::FieldDescriptorProto& f, const pb::DescriptorProto& msg) const {
        using proto23::options::Inplace;
        const auto label = static_cast<std::int32_t>(f.label);
        const auto type  = static_cast<std::int32_t>(f.type);
        const bool rep   = (label == F::LABEL_REPEATED);
        bool inner {};
        pb::FieldOptions options = make_options(type, rep, f.options.value_or(pb::FieldOptions{}), options_);
        if (type == F::TYPE_MESSAGE) {
            auto it = reg_.find(f.type_name);
            if (it != reg_.end()) {
              // Map field: repeated message with map_entry option
              if (rep && is_map_entry(it->second))
                  return { map_type(*it->second), false };
              inner = is_inner(it->second, &msg);
            }
        }
        auto element_type_name = element_type(f, file_.package);
        const auto [complete, noinit] = (type == F::TYPE_MESSAGE) ? is_complete(element_type_name) : traits{true, false};
        if (type == F::TYPE_MESSAGE) {
           element_type_name += sanitizing_suffix(element_type_name);
        }
        if (rep) {
            // repeated bytes -> vector of blobs (each decoded as a separate LEN)
            if (type == F::TYPE_BYTES) {
                return { "std::vector<std::vector<std::byte>>", false };
            } else {
                if (options.inplace == Inplace::YES && complete) {
                    return { "proto23::inplace_vector<" + element_type_name + ", " + std::to_string(options.capacity) + ">",
                            false };
                } else {
                  return { "std::vector<" + element_type_name + ">", !complete || noinit };
                }
            }
        }

        // PDO non-repeated message fields are generated in-place,
        // all other non-repeated message fields -> std::unique_ptr<T>.
        // This handles both regular message fields and proto3-optional message
        // fields.  std::unique_ptr<T> is valid with a forward-declared (incomplete)
        // T, which is necessary because we forward-declare all top-level message
        // types at the top of the generated header.
        if (type == F::TYPE_MESSAGE) {
            if (complete && (options.inplace != Inplace::NO) && (options.inplace == Inplace::YES || inner)) {
                if (f.proto3_optional) {
                    return { "std::optional<" + element_type_name + ">", noinit };
                } else {
                    return { element_type_name, noinit };
                }
            } else {
                return { "std::unique_ptr<" + element_type_name + ">", !complete || noinit };
            }
        }
        if (options.inplace == Inplace::YES) {
            if (type == F::TYPE_STRING) {
                return { "proto23::inplace_string<" + std::to_string(options.capacity) + ">", false };
            }
            if (type == F::TYPE_BYTES) {
                return { "proto23::inplace_vector<std::byte," + std::to_string(options.capacity) + ">", false };
            }
        }
        // proto3-optional scalar / enum -> std::optional<T>
        if (f.proto3_optional) {
            return { "std::optional<" + element_type_name + ">", !complete || noinit };
        }
        return { element_type_name, ! complete || noinit };
    }

    // ---- Emitters ----

    void emit_enum(std::ostream& out, const pb::EnumDescriptorProto& e, int depth) const {
        out << ind(depth) << "enum class " << e.name << sanitizing_suffix(e.name) << " : std::int32_t {\n";
        for (const auto& v : e.value) {
            out << ind(depth + 1)
                << v.name << sanitizing_suffix(v.name) << " = "
                << static_cast<std::int32_t>(v.number) << ",\n";
        }
        out << ind(depth) << "};\n\n";
    }
    /// Emit [[deprecated]] attribute
    static constexpr std::string_view deprecated(const auto& options) {
        return (options && options->deprecated) ? "[[deprecated]]" : "";
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
                      int                           depth) {
        // Auto-generated map-entry messages are not emitted directly;
        // they are represented as std::map<K,V> fields in the parent.
        if (is_map_entry(&msg)) { return; }

        bool noinit = false;
        auto msg_name = msg.name;
        msg_name += sanitizing_suffix(msg.name);
        out << ind(depth) << deprecated(msg.options) << "struct " << msg_name << " {\n";

        // Nested enums
        for (const auto& e : msg.enum_type) {
            emit_enum(out, e, depth + 1);
        }

        // Nested messages (recursive).  Their deferred-dtor entries are
        // collected into the SAME deferred_dtors vector so that they are
        // all emitted at namespace scope after every type is complete.
        const std::string child_prefix = qual_prefix + msg_name + "::";
        for (const auto& nm : msg.nested_type) {
            emit_message(out, nm, child_prefix, depth + 1);
        }

        // Build oneof groups: oneof_index -> ordered field list
        // (skip proto3-optional synthetic oneofs)
        std::map<std::int32_t,
                 std::vector<const pb::FieldDescriptorProto*>> oneof_groups {};
        for (const auto& f : msg.field) {
            if (f.oneof_index.has_value() && !f.proto3_optional) {
                oneof_groups[static_cast<std::int32_t>(*f.oneof_index)].push_back(&f);
            }
        }

        // Field declarations — iterate in proto order
        std::set<std::int32_t> emitted {};
        for (const auto& f : msg.field) {
            if (f.oneof_index.has_value() && !f.proto3_optional) {
                // Real oneof: emit as std::variant on first encounter
                const auto oidx = static_cast<std::int32_t>(*f.oneof_index);
                if (emitted.count(oidx) != 0U) { continue; }
                emitted.insert(oidx);

                const auto& oname =
                    msg.oneof_decl.at(static_cast<std::size_t>(oidx)).name;
                const auto& ofields = oneof_groups.at(oidx);

                out << ind(depth + 1) << deprecated(f.options) << "std::variant<";
                bool items_complete {true};
                bool items_noinit {false};
                for (std::size_t i = 0U; i < ofields.size(); ++i) {
                    if (i != 0U) { out << ", "; }
                    const auto item_type_name = element_type(*ofields[i], file_.package);
                    const auto item_traits = is_complete(item_type_name);
                    items_complete = items_complete && item_traits.complete;
                    items_noinit = items_noinit || item_traits.noinit;
                    if (ofields[i]->type == F::TYPE_MESSAGE && !item_traits.complete) {
                      out << "std::unique_ptr<" << item_type_name << ">";
                    } else {
                      out << item_type_name;
                    }
                }
                out << "> " << oname << sanitizing_suffix(oname)
                    << (items_complete && !items_noinit ? "{}" : "") << ";\n";
            } else {
                auto [field_type, field_noinit] = field_cpp_type(f, msg);
                out << ind(depth + 1) << field_type << " " << f.name << sanitizing_suffix(f.name)
                    << (field_noinit ? ";\n" : "{};\n");
                if (field_noinit) noinit = true;
            }
        }

        if (msg.field.empty()) {
          out << ind(depth + 1) << "// This message has no fields\n";
        } else {
            // using Model = proto23::Fields<...>
            out << "\n" << ind(depth + 1) << "using Model = proto23::Fields<\n";

            emitted.clear();
            bool first = true;

            auto comma = [&first, &out]() {
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
                    out << ind(depth + 2) << "proto23::OneOf<&" << msg_name << "::" << oname << sanitizing_suffix(oname);
                    for (const auto* of : ofields) {
                        out << ", " << static_cast<std::int32_t>(of->number);
                    }
                    out << ">";
                } else {
                    comma();
                    out << ind(depth + 2) << "proto23::Field<&" << msg_name << "::" << f.name << sanitizing_suffix(f.name)
                        << ", " << static_cast<std::int32_t>(f.number)
                        << packed_attr(f.label == F::LABEL_REPEATED, f.options) << ">";
                }
            }
            out << ">;\n";
        }
        if (msg.options.value_or(pb::MessageOptions{}).message_id != 0) {
          if (depth == 0) {
            out << ind(depth + 1) << "static constexpr auto MESSAGE_ID = static_cast<"
                << file_.package << "::MessageTypeID>(" << msg.options->message_id << ");\n";
          } else {
            out << ind(depth + 1) << "// MESSAGE_ID ignored for nested messages\n";
            // TODO report warning
          }
        }
        out << ind(depth) << "};\n\n";
        complete(qual_prefix + msg.name, noinit);
    }

    /// Convert "a.b.c" package string to "a::b::c" C++ namespace.
    static std::string pkg_to_ns(const std::string& pkg) {
        std::string ns{};
        ns.reserve(pkg.size() + 4U);
        for (const char c : pkg) {
            if (c == '.') { ns += "::"; }
            else          { ns += c; }
        }
        return ns;
    }
};

// ---- Generator::generate() ----

std::string Generator::generate() {
    std::ostringstream out{};
    const auto& pkg = file_.package;
    options_ = file_.options.value_or(default_options);
    if (options_.default_string_capacity == 0)
        options_.default_string_capacity = default_options.default_string_capacity;
    if (options_.default_repeated_capacity == 0)
        options_.default_repeated_capacity = default_options.default_repeated_capacity;
    // --- File header ---
    out << "// Generated by protoc-gen-proto23 -- DO NOT EDIT\n"
        << "// Source: " << file_.name << "\n\n"
        << "#pragma once\n\n"
        << "#include <proto23/proto23.h>\n\n"
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
            out << "struct " << m.name << sanitizing_suffix(m.name) << ";\n";
        }
    }
    if (!file_.message_type.empty()) { out << "\n"; }

    // File-level enum definitions
    for (const auto& e : file_.enum_type) {
        emit_enum(out, e, 0);
    }

    // Message struct definitions
    for (const auto& m : file_.message_type) {
        emit_message(out, m, /*qual_prefix=*/"", /*depth=*/0);
    }

    // --- Namespace close ---
    if (!ns.empty()) { out << "} // namespace " << ns << "\n"; }

    return out.str();
}

// ============================================================================
// Output filename
// ============================================================================

static std::string output_filename(const std::string& proto_name) {
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

int main() {
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
    try {
      pb::CodeGeneratorRequest request{};
      const auto result = proto23::deserialize(iss, request);
      if( result.errors() != proto23::parse_error::none) {
        std::cerr << "Parsed with errors\n" 
                  << proto23::diagnostic::explain(result.errors()) << '\n';
      }

      // Index proto_file by name for quick lookup
      std::map<std::string, const pb::FileDescriptorProto*> file_map{};
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
      std::ostringstream oss{};
      proto23::serialize(oss, response);
      const auto output = oss.str();
      std::cout.write(output.data(), static_cast<std::streamsize>(output.size()));
      std::cout.flush();
    } catch(const std::exception& e) {
      std::cerr << e.what() << '\n';
      return 1;
    } catch(...) {
      std::cerr << "Unknown exception\n";
      return 1;
    }

    return 0;
}
