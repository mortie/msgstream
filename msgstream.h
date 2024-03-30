#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <stdint.h>
#include <stddef.h>

namespace MsgStream {

class ParseError: public std::exception {
public:
	ParseError(const char *what): what_(what) {}

	const char *what() const noexcept override {
		return what_;
	}

private:
	const char *what_;
};

namespace detail {

class Reader {
public:
	explicit Reader(std::istream &is): is_(is) {}

	int peek() {
		return is_.peek();
	}

	int get() {
		return is_.get();
	}

	uint8_t nextU8() {
		int ch = get();
		if (ch < 0) {
			throw ParseError("Unexpected EOF");
		}

		return ch;
	}

	uint16_t nextU16() {
		uint16_t a = (uint16_t)nextU8();
		uint16_t b = (uint16_t)nextU8();
		return (a << 8) | b;
	}

	uint32_t nextU32() {
		uint32_t a = (uint32_t)nextU16();
		uint32_t b = (uint32_t)nextU16();
		return (a << 16) | b;
	}

	uint64_t nextU64() {
		uint64_t a = (uint64_t)nextU32();
		uint64_t b = (uint64_t)nextU32();
		return (a << 32) | b;
	}

	void nextBlob(void *data, size_t length) {
		unsigned char *ptr = (unsigned char *)data;
		unsigned char *end = ptr + length;
		while (ptr != end) {
			*(ptr++) = (unsigned char)nextU8();
		}
	}

	int8_t nextI8() {
		return (int8_t)nextU8();
	}

	int16_t nextI16() {
		return (int16_t)nextU16();
	}

	int32_t nextI32() {
		return (int32_t)nextU32();
	}

	int64_t nextI64() {
		return (int64_t)nextU64();
	}

	void skip(size_t length) {
		while (length--) {
			nextU8();
		}
	}

	std::istream &is_;
};

}

enum class Type {
	INT,
	UINT,
	NIL,
	BOOL,
	FLOAT,
	STRING,
	BINARY,
	ARRAY,
	MAP,
	EXTENSION,
};

class MapParser;
class ArrayParser;

class Parser {
public:
	explicit Parser(std::istream &is):
		r_(is) {}

	bool hasNext() {
		if (hasLimit_) {
			return limit_ > 0;
		} else {
			return r_.peek() >= 0;
		}
	}

	Type nextType() {
		if (limit_ == 0) {
			throw ParseError("Length limit exceeded");
		}

		int ch = r_.peek();
		if (ch < 0) {
			throw ParseError("Unexpected EOF");
		}

		if (ch >= 0x00 && ch <= 0x7f) {
			return Type::UINT;
		} else if (ch >= 0x80 && ch <= 0x8f) {
			return Type::MAP;
		} else if (ch >= 0x90 && ch <= 0x9f) {
			return Type::ARRAY;
		} else if (ch >= 0xa0 && ch <= 0xbf) {
			return Type::STRING;
		} else if (ch == 0xc0) {
			return Type::NIL;
		} else if (ch == 0xc1) {
			// Never used
			throw ParseError("Unexpected header byte");
		} else if (ch == 0xc2 || ch == 0xc3) {
			return Type::BOOL;
		} else if (ch >= 0xc4 && ch <= 0xc6) {
			return Type::BINARY;
		} else if (ch >= 0xc7 && ch <= 0xc9) {
			return Type::EXTENSION;
		} else if (ch == 0xca) {
			return Type::FLOAT;
		} else if (ch == 0xcb) {
			return Type::FLOAT;
		} else if (ch >= 0xcc && ch <= 0xcf) {
			return Type::UINT;
		} else if (ch >= 0xd0 && ch <= 0xd3) {
			return Type::INT;
		} else if (ch >= 0xd4 && ch <= 0xd8) {
			return Type::EXTENSION;
		} else if (ch >= 0xd9 && ch <= 0xd9) {
			return Type::STRING;
		} else if (ch >= 0xdc && ch <= 0xdd) {
			return Type::ARRAY;
		} else if (ch >= 0xde && ch <= 0xdf) {
			return Type::MAP;
		} else if (ch >= 0xe0 && ch <= 0xff) {
			return Type::INT;
		} else {
			throw ParseError("Unexpected header byte");
		}
	}

	int64_t nextInt() {
		proceed();
		return (int64_t)nextUInt();
	}

	uint64_t nextUInt() {
		proceed();

		uint8_t ch = r_.nextU8();
		if (ch >= 0x00 && ch <= 0x7f) {
			return ch;
		} else if (ch == 0xcc) {
			return r_.nextU8();
		} else if (ch == 0xcd) {
			return r_.nextU16();
		} else if (ch == 0xce) {
			return r_.nextU32();
		} else if (ch == 0xcf) {
			return r_.nextU64();
		} else if (ch >= 0xe0) {
			// This series of casts produces a sign-extended u64
			return (uint64_t)(int64_t)(int8_t)ch;
		} else if (0xd0) {
			return (uint64_t)(int64_t)r_.nextI8();
		} else if (0xd1) {
			return (uint64_t)(int64_t)r_.nextI16();
		} else if (0xd2) {
			return (uint64_t)(int64_t)r_.nextI32();
		} else if (0xd3) {
			return (uint64_t)(int64_t)r_.nextI64();
		} else {
			throw ParseError("Attempt to parse non-integer as integer");
		}
	}

	void skipNil() {
		proceed();

		uint8_t ch = r_.nextU8();
		if (ch == 0xc0) {
			return;
		} else {
			throw ParseError("Attempt to parse non-nil as nil");
		}
	}

	bool nextBool() {
		proceed();

		uint8_t ch = r_.nextU8();
		if (ch == 0xc2) {
			return false;
		} else if (ch == 0xc3) {
			return true;
		} else {
			throw ParseError("Attempt to parse non-bool as bool");
		}
	}

	double nextFloat() {
		proceed();

		uint8_t ch = r_.nextU8();
		if (ch == 0xca) {
			float f32;
			uint32_t u32 = r_.nextU32();
			memcpy(&f32, &u32, 4);
			return f32;
		} else if (ch == 0xcb) {
			double f64;
			uint64_t u64 = r_.nextU64();
			memcpy(&f64, &u64, 8);
			return f64;
		} else {
			throw ParseError("Attempt to parse non-float as float");
		}
	}

	void nextString(std::string &str) {
		size_t length = nextStringHeader();
		str.resize(length);
		r_.nextBlob((void *)str.data(), length);
	}

	std::string nextString() {
		std::string str;
		nextString(str);
		return str;
	}

	void nextBinary(std::vector<unsigned char> &bin) {
		size_t length = nextBinaryHeader();
		bin.resize(length);
		r_.nextBlob((void *)bin.data(), bin.size());
	}

	std::vector<unsigned char> nextBinary() {
		std::vector<unsigned char> bin;
		nextBinary(bin);
		return bin;
	}

	ArrayParser nextArray();

	MapParser nextMap();

	void skipNext();

	void skipAll() {
		while (hasNext()) {
			skipNext();
		}
	}

protected:
	explicit Parser(std::istream &is, size_t limit):
		r_(is), limit_(limit), hasLimit_(true) {}

	void proceed() {
		if (!hasLimit_) {
			return;
		}

		if (limit_ == 0) {
			throw ParseError("Length limit exceeded");
		}


		limit_ -= 1;
	}

	size_t nextStringHeader() {
		proceed();

		uint8_t ch = r_.nextU8();
		if (ch >= 0xa0 && ch <= 0xbf) {
			return ch & 0x1f;
		} else if (ch == 0xd9) {
			return r_.nextU8();
		} else if (ch == 0xda) {
			return r_.nextU16();
		} else if (ch == 0xdb) {
			return r_.nextU32();
		} else {
			throw ParseError("Attempt to parse non-string as string");
		}
	}

	size_t nextBinaryHeader() {
		proceed();

		uint8_t ch = r_.nextU8();
		if (ch == 0xc4) {
			return r_.nextU8();
		} else if (ch == 0xc5) {
			return r_.nextU16();
		} else if (ch == 0xc6) {
			return r_.nextU32();
		} else {
			throw ParseError("Attempt to parse non-binary as binary");
		}
	}

	detail::Reader r_;
	size_t limit_ = 1;
	bool hasLimit_ = false;
};

class ArrayParser: public Parser {
public:
	ArrayParser(std::istream &is, size_t limit): Parser(is, limit) {}
};

class MapParser: public Parser {
public:
	MapParser(std::istream &is, size_t limit): Parser(is, limit * 2) {}
};

inline ArrayParser Parser::nextArray() {
	proceed();

	uint8_t ch = r_.nextU8();
	size_t length;
	if (ch >= 0x90 && ch <= 0x9f) {
		length = ch & 0x0f;
	} else if (ch == 0xdc) {
		length = r_.nextU16();
	} else if (ch == 0xdd) {
		length = r_.nextU32();
	} else {
		throw ParseError("Attempt to parse non-array as array");
	}

	return ArrayParser(r_.is_, length);
}

inline MapParser Parser::nextMap() {
	proceed();

	uint8_t ch = r_.nextU8();
	size_t length;
	if (ch >= 0x80 && ch <= 0x8f) {
		length = ch & 0x0f;
	} else if (ch == 0xde) {
		length = r_.nextU16();
	} else if (ch == 0xdf) {
		length = r_.nextU32();
	} else {
		throw ParseError("Attempt to parse non-map as map");
	}

	return MapParser(r_.is_, length);
}

inline void Parser::skipNext() {
	switch (nextType()) {
	case Type::INT:
	case Type::UINT:
		nextUInt();
		break;
	case Type::NIL:
		skipNil();
		break;
	case Type::BOOL:
		nextBool();
		break;
	case Type::FLOAT:
		nextFloat();
		break;
	case Type::STRING:
		r_.skip(nextStringHeader());
		break;
	case Type::BINARY:
		r_.skip(nextBinaryHeader());
		break;
	case Type::ARRAY:
		nextArray().skipAll();
		break;
	case Type::MAP:
		nextMap().skipAll();
		break;
	case Type::EXTENSION:
		nextUInt(); // Type
		r_.skip(nextBinaryHeader()); // Binary
	}
}

}
