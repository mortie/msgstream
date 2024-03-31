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

## Tests

Run tests with `make check`. This depends on git.
This will download 
[kawanet's msgpack-test-suite](https://github.com/kawanet/msgpack-test-suite/),
a large list of msgpack strings and their associated expected values.

All tests pass.

## Fuzzing

Install [AFLplusplus](https://aflplus.plus/)
(`sudo apt install afl++` on Ubuntu),
then run `make fuzz` to start fuzzing.

By default, 20 fuzzer processes will start.
You can change this in the [fuzz.sh](./fuzz.sh) script.

The fuzz tester tests the `msgpack-to-json` program.
The goal is to make sure that no input causes crashes;
invalid input should cause exceptions to be thrown
in a controled manner.
