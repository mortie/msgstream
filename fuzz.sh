#!/bin/sh

set -e

rm -rf fuzz-data
mkdir -p fuzz-data
make CXX=afl-c++ OUT=./fuzz-data ./fuzz-data/msgpack-to-json
make OUT=./fuzz-data ./fuzz-data/json-to-msgpack

cd fuzz-data
mkdir -p seeds
echo '100' | ./json-to-msgpack > seeds/number.json
echo '"Hello World"' | ./json-to-msgpack > seeds/string.json
echo '[10, 20]' | ./json-to-msgpack > seeds/array.json
echo '[{"hello": "world"}, 33]' | ./json-to-msgpack > seeds/object.json

mkdir -p output
afl-fuzz -i seeds -o output -M "msgcpp-fuzzer-1" -- ./msgpack-to-json &
for i in $(seq 2 20); do
	afl-fuzz -i seeds -o output -S "msgcpp-fuzzer-$i" -- ./msgpack-to-json >/dev/null &
done

wait
