# MsgStream

MsgStream is a streaming MessagePack parser and serializer for C++.

## Dependencies

MsgStream doesn't depend on any external libraries,
but depends on these new-ish parts of the stdlib:

* `std::span` (from `<span>`, C++20)
* `std::string_view` (from `<string_view>`, C++17)

This means you probably need to use your compiler's C++20 mode
(`-std=c++20`, or `-std=gnu++20` for GNU extensions)
to use MsgStream.

In addition, the `json-to-msgpack` example program depends
on [JsonCpp](https://github.com/open-source-parsers/jsoncpp)
for JSON parsing.

## API

The API is split into two main classes:
`MsgStream::Parser` for parsing a MessagePack stream,
and `MsgStream::Serializer` for serializing MessagePack messages.

To get a feel for how the API works, I recommend taking a look at
the example programs:

* [examples/msgpack-to-json.cc](examples/msgpack-to-json.cc):
  Parse a MessagePack file and output (almost correct) JSON
* [examples/json-to-msgpack.cc](examples/json-to-msgpack.cc):
  Parse a JSON file and output MessagePack

[msgstream.h](msgstream.h) contains documentation comments.
