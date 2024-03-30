all: msgpack-to-json json-to-msgpack

msgpack-to-json: examples/msgpack-to-json.cc msgstream.h
	$(CXX) -o $@ $< -std=c++20 -Wall -Wextra -Wpedantic

json-to-msgpack: examples/json-to-msgpack.cc msgstream.h
	$(CXX) -o $@ $< -std=c++20 -Wall -Wextra -Wpedantic \
		$(shell pkg-config --libs --cflags jsoncpp)

.PHONY: clean
clean:
	rm -f msgpack-to-json
