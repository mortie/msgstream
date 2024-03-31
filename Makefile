OUT ?= .

all: $(OUT)/msgpack-to-json $(OUT)/json-to-msgpack

$(OUT)/msgpack-to-json: examples/msgpack-to-json.cc msgstream.h
	$(CXX) -o $@ $< -std=c++20 -Wall -Wextra -Wpedantic

$(OUT)/json-to-msgpack: examples/json-to-msgpack.cc msgstream.h
	$(CXX) -o $@ $< -std=c++20 -Wall -Wextra -Wpedantic \
		$(shell pkg-config --libs --cflags jsoncpp)

.PHONY: clean
clean:
	rm -f $(OUT)/msgpack-to-json $(OUT)/json-to-msgpack
