msgpack-to-json: examples/msgpack-to-json.cc msgstream.h
	$(CXX) -o $@ $< -std=c++11

.PHONY: clean
clean:
	rm -f msgpack-to-json
