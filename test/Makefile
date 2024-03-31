.PHONY: al
all: test sanitizers-test

msgpack-test-suite/dist/msgpack-test-suite.json:
	git clone https://github.com/kawanet/msgpack-test-suite.git
	cd msgpack-test-suite && git checkout e04f6edeaae589c768d6b70fcce80aa786b7800e
	touch $@

sanitizers-test: test.cc ../msgstream.h
	$(CXX) -o $@ $< \
		-std=c++20 -Wall -Wextra -Wpedantic -Werror \
		-fsanitize=address,undefined \
		$(shell pkg-config --libs --cflags jsoncpp)

test: test.cc ../msgstream.h
	$(CXX) -o $@ $< \
		-std=c++20 -Wall -Wextra -Wpedantic -Werror \
		$(shell pkg-config --libs --cflags jsoncpp)

.PHONY: check
check: sanitizers-test msgpack-test-suite/dist/msgpack-test-suite.json
	./sanitizers-test msgpack-test-suite/dist/msgpack-test-suite.json

.PHONY: valgrind-check
valgrind-check: test msgpack-test-suite/dist/msgpack-test-suite.json
	valgrind ./test msgpack-test-suite/dist/msgpack-test-suite.json

.PHONY: clean
clean:
	rm -f test

.PHONY: cleanall
cleanall: clean
	rm -rf msgpack-test-suite
