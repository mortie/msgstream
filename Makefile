OUT ?= .

.PHONY: all
all: $(OUT)/msgpack-to-json $(OUT)/json-to-msgpack

$(OUT)/msgpack-to-json: examples/msgpack-to-json.cc msgstream.h
	$(CXX) -o $@ $< -std=c++20 -Wall -Wextra -Wpedantic

$(OUT)/json-to-msgpack: examples/json-to-msgpack.cc msgstream.h
	$(CXX) -o $@ $< -std=c++20 -Wall -Wextra -Wpedantic \
		$(shell pkg-config --libs --cflags jsoncpp)

.PHONY: fuzz
fuzz:
	./fuzz.sh

.PHONY: check
check:
	make -C test check

.PHONY: valgrind-check
valgrind-check:
	make -C test valgrind-check

.PHONY: clean
clean:
	rm -f $(OUT)/msgpack-to-json $(OUT)/json-to-msgpack
	rm -rf fuzz-data
	make -C test clean

.PHONY: cleanall
cleanall: clean
	make -C test cleanall
