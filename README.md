# proto23 - C++23 Protobuf serializer/deserializer with explicit field listing
Backport of proto26 (C++26 reflection-based) to C++23.

Instead of `[[=N]]` reflection annotations, each serializable struct declares:
```
   using Model = proto::Fields<proto::Field<&Foo::x, 1>, proto::Field<&Foo::y, 2>, ...>;
```

Wire format and semantics are identical to the C++26 implementation.

Licensed under MIT License, see full text in LICENSE or visit page https://opensource.org/license/mit/
