#include <iostream>
#include <span>
#include <sstream>
#include <exception>
#include <string>
#include <string_view>
#include <vector>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

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

class SerializeError: public std::exception {
public:
	SerializeError(const char *what): what_(what) {}

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

	void nextBlob(void *data, size_t length) {
		unsigned char *ptr = (unsigned char *)data;
		unsigned char *end = ptr + length;
		while (ptr != end) {
			*(ptr++) = (unsigned char)nextU8();
		}
	}

	void skip(size_t length) {
		while (length--) {
			nextU8();
		}
	}

	std::istream &is_;
};

class Writer {
public:
	explicit Writer(std::ostream &os): os_(os) {}

	void writeU8(uint8_t num) {
		os_.put((char)num);
	}

	void writeU16(uint16_t num) {
		writeU8(num >> 8);
		writeU8(num & 0xffu);
	}

	void writeU32(uint32_t num) {
		writeU16(num >> 16);
		writeU16(num & 0xffffu);
	}

	void writeU64(uint64_t num) {
		writeU32(num >> 32);
		writeU32(num & 0xffffffffu);
	}

	void writeI8(int8_t num) {
		writeU8((uint8_t)num);
	}

	void writeI16(int16_t num) {
		writeU16((uint16_t)num);
	}

	void writeI32(int32_t num) {
		writeU32((uint32_t)num);
	}

	void writeI64(int64_t num) {
		writeU64((int64_t)num);
	}

	void writeBlob(const void *data, size_t length) {
		unsigned char *ptr = (unsigned char *)data;
		unsigned char *end = ptr + length;
		while (ptr != end) {
			writeU8(*(ptr++));
		}
	}

	std::ostream &os_;
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

/**
 * A MessagePack stream parser.
 * Methods will throw a ParseError if preconditions are violated.
 */
class Parser {
public:
	explicit Parser(std::istream &is):
		r_(is) {}

	/**
	 * Check whether there are more objects available in the stream.
	 * For unconstrained parsers, this returns 'false' only when EOF is reached.
	 * For constrained parsers (i.e MapParser and ArrayParser),
	 * this returns 'false' when there are no more values left to read
	 * in the array/map.
	 *
	 * Preconditions: None
	 */
	bool hasNext() {
		if (hasLimit_) {
			return limit_ > 0;
		} else {
			return r_.peek() >= 0;
		}
	}

	/**
	 * Get the type of the next value.
	 *
	 * Preconditions:
	 *   The stream cursor must be at the start of a valid object
	 *   hasNext() == true
	 */
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

	/**
	 * Get the next value as an integer.
	 *
	 * Preconditions:
	 *   hasNext() == true
	 *   nextType() == Type::INT || nextType() == Type::UINT
	 *
	 * Note: if nextType() == Type::UINT,
	 * the int might not fit in an int64_t.
	 * This will cause the value to wrap around.
	 */
	int64_t nextInt() {
		proceed();
		return (int64_t)nextUInt();
	}

	/**
	 * Get the next value as an unsigned integer.
	 *
	 * Preconditions:
	 *   hasNext() == true
	 *   nextType() == Type::INT || nextType() == Type::UINT
	 *
	 * Note: if nextType() == Type::INT,
	 * the int might not fit in a uint64_t.
	 * This will cause the value to wrap around.
	 */
	uint64_t nextUInt() {
		proceed();

		uint8_t ch = r_.nextU8();
		if (ch <= 0x7fu) {
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

	/**
	 * Skip the next value if it's a nil.
	 *
	 * Preconditions:
	 *   hasNext() == true
	 *   nextType() == Type::NIL
	 */
	void skipNil() {
		proceed();

		uint8_t ch = r_.nextU8();
		if (ch == 0xc0) {
			return;
		} else {
			throw ParseError("Attempt to parse non-nil as nil");
		}
	}


	/**
	 * Get the next value as a boolean.
	 *
	 * Preconditions:
	 *   hasNext() == true
	 *   nextType() == Type::BOOL
	 */
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

	/**
	 * Get the next value as a float.
	 *
	 * Preconditions:
	 *   hasNext() == true
	 *   nextType() == Type::FLOAT
	 */
	double nextFloat() {
		proceed();

		uint8_t ch = r_.nextU8();
		if (ch == 0xca) {
			static_assert(sizeof(float) == sizeof(uint32_t));
			float f32;
			uint32_t u32 = r_.nextU32();
			memcpy(&f32, &u32, 4);
			return f32;
		} else if (ch == 0xcb) {
			static_assert(sizeof(double) == sizeof(uint64_t));
			double f64;
			uint64_t u64 = r_.nextU64();
			memcpy(&f64, &u64, 8);
			return f64;
		} else {
			throw ParseError("Attempt to parse non-float as float");
		}
	}

	/**
	 * Get the next value as a string.
	 *
	 * Preconditions:
	 *   hasNext() == true
	 *   nextType() == Type::STRING
	 */
	void nextString(std::string &str) {
		size_t length = nextStringHeader();
		str.resize(length);
		r_.nextBlob((void *)str.data(), length);
	}

	/**
	 * Like 'nextString(std::string &)',
	 * except that a new string is returned instead.
	 */
	std::string nextString() {
		std::string str;
		nextString(str);
		return str;
	}

	/**
	 * Get the next value as a byte string.
	 *
	 * Preconditions:
	 *   hasNext() == true
	 *   nextType() == Type::BINARY
	 */
	void nextBinary(std::vector<unsigned char> &bin) {
		size_t length = nextBinaryHeader();
		bin.resize(length);
		r_.nextBlob((void *)bin.data(), bin.size());
	}

	/**
	 * Like 'nextBinary(std::vector<unsigned char> &)',
	 * except that a new vector is returned instead.
	 */
	std::vector<unsigned char> nextBinary() {
		std::vector<unsigned char> bin;
		nextBinary(bin);
		return bin;
	}

	/**
	 * Create a constrained sub-parser limited to read
	 * only the values in the next array value.
	 *
	 * Preconditions:
	 *   hasNext() == true
	 *   nextType() == Type::ARRAY
	 */
	ArrayParser nextArray();

	/**
	 * Create a constrained sub-parser limited to read
	 * only the values in the next map value.
	 *
	 * Preconditions:
	 *   hasNext() == true
	 *   nextType() == Type::MAP
	 */
	MapParser nextMap();

	/**
	 * Read the next extension value.
	 * Will populate 'ext' with its contents,
	 * and return the extension value's type.
	 *
	 * Preconditions:
	 *   hasNext() == true
	 *   nextType() == Type::EXTENSION
	 */
	int64_t nextExtension(std::vector<unsigned char> &ext) {
		int64_t type;
		size_t length;
		nextExtensionHeader(type, length);
		ext.resize(length);
		r_.nextBlob((void *)ext.data(), ext.size());
		return type;
	}

	/**
	 * Skip the next value, whatever its type.
	 *
	 * Preconditions:
	 *   The stream cursor must be at the start of a valid object
	 *   hasNext() == true
	 */
	void skipNext();

	/**
	 * Skip all available values.
	 *
	 * Preconditions:
	 *   The stream must be a valid MessagePack stream.
	 */
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

	void nextExtensionHeader(int64_t &type, size_t &length) {
		if (limit_ == 0) {
			throw ParseError("Length limit exceeded");
		}

		uint8_t ch = r_.nextU8();
		if (ch == 0xd4) {
			length = 1;
		} else if (ch == 0xd5) {
			length = 2;
		} else if (ch == 0xd6) {
			length = 4;
		} else if (ch == 0xd7) {
			length = 8;
		} else if (ch == 0xd8) {
			length = 16;
		} else if (ch == 0xc7) {
			length = r_.nextU8();
		} else if (ch == 0xc8) {
			length = r_.nextU16();
		} else if (ch == 0xc9) {
			length = r_.nextU32();
		} else {
			throw ParseError("Attempt to parse non-extension as extension");
		}

		type = nextInt();
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
	case Type::EXTENSION: {
		int64_t type;
		size_t length;
		nextExtensionHeader(type, length);
		r_.skip(length);
	}
		break;
	}
}

class ArrayBuilder;
class MapBuilder;

/**
 * A MessagePack stream writer.
 */
class Serializer {
public:
	explicit Serializer(std::ostream &os):
		w_(os) {}

	/**
	 * Write an integer value.
	 */
	void writeInt(int64_t num) {
		proceed();

		if (num >= 0 && num <= 0x7f) {
			w_.writeU8(num);
		} else if (num >= -32 && num < -1) {
			w_.writeI8(num);
		} else if (num >= -128 && num <= 127) {
			w_.writeU8(0xd0);
			w_.writeI8(num);
		} else if (num >= -32768 && num <= 32767) {
			w_.writeU8(0xd1);
			w_.writeI16(num);
		} else if (num >= -2147483648 && num <= 2147483647) {
			w_.writeU8(0xd2);
			w_.writeI32(num);
		} else {
			w_.writeU8(0xd3);
			w_.writeI64(num);
		}
	}

	/**
	 * Write an unsigned integer value.
	 */
	void writeUInt(uint64_t num) {
		proceed();

		if (num <= 0x7fu) {
			w_.writeU8(num);
		} else if (num <= 0xffu) {
			w_.writeU8(0xcc);
			w_.writeU8(num);
		} else if (num <= 0xffffu) {
			w_.writeU8(0xcd);
			w_.writeU16(num);
		} else if (num <= 0xffffffffu) {
			w_.writeU8(0xce);
			w_.writeU32(num);
		} else {
			w_.writeU8(0xcf);
			w_.writeU64(num);
		}
	}

	/**
	 * Write a nil value.
	 */
	void writeNil() {
		proceed();
		w_.writeU8(0xc0);
	}

	/**
	 * Write a bool value.
	 */
	void writeBool(bool b) {
		proceed();
		w_.writeU8(b ? 0xc3 : 0xc2);
	}

	/**
	 * Write a 32-bit floating point value.
	 */
	void writeFloat32(float f) {
		proceed();
		static_assert(sizeof(float) == sizeof(uint32_t));
		uint32_t num;
		memcpy(&num, &f, sizeof(num));
		w_.writeU32(num);
	}

	/**
	 * Write a 64-bit floating point value.
	 */
	void writeFloat64(double d) {
		proceed();
		static_assert(sizeof(double) == sizeof(uint64_t));
		uint64_t num;
		memcpy(&num, &d, sizeof(num));
		w_.writeU64(num);
	}

	/**
	 * Write a string value.
	 * The string must be smaller than 2^31-1 bytes long.
	 */
	void writeString(std::string_view sv) {
		proceed();
		size_t length = sv.size();
		if (length <= 0x1fu) {
			w_.writeU8(0xa0u | length);
		} else if (length <= 0xffu) {
			w_.writeU8(0xd9);
			w_.writeU8(length);
		} else if (length <= 0xffffu) {
			w_.writeU8(0xda);
			w_.writeU16(length);
		} else if (length <= 0xffffffffu) {
			w_.writeU8(0xdb);
			w_.writeU32(length);
		} else {
			throw SerializeError("String too long");
		}

		w_.writeBlob((const void *)sv.data(), length);
	}

	/**
	 * Write a byte string value.
	 * The byte string must be smaller than 2^31-1 bytes long.
	 */
	void writeBinary(std::span<unsigned char> bv) {
		proceed();
		size_t length = bv.size();
		if (length <= 0xffu) {
			w_.writeU8(0xc4);
			w_.writeU8(length);
		} else if (length <= 0xffffu) {
			w_.writeU8(0xc5);
			w_.writeU16(length);
		} else if (length <= 0xffffffffu) {
			w_.writeU8(0xc6);
			w_.writeU32(length);
		} else {
			throw SerializeError("Binary too long");
		}

		w_.writeBlob(bv.data(), length);
	}

	/**
	 * Write an array value.
	 * Will clear the ArrayBuilder.
	 * The ArrayBuilder can be re-used to create another array value.
	 */
	void writeArray(ArrayBuilder &ab);

	/**
	 * Begin writing an array value.
	 * Returns a sub-serializer which array values must be written to.
	 * Exactly 'n' values must be written to the sub-serializer,
	 * and then 'endArray' must be called.
	 */
	Serializer beginArray(size_t n) {
		proceed();
		nesting_ = true;
		nestingLength_ = n;
		writeArrayHeader(n);
		return Serializer(w_.os_);
	}

	/**
	 * Complete writing an array that was started by 'beginArray'.
	 * The same amount of values must have been written to the serializer
	 * as were passed to the 'beginArray' method.
	 */
	void endArray(Serializer &sub) {
		if (sub.written() != nestingLength_) {
			throw SerializeError("beginArray/endArray length mismatch");
		}

		nesting_ = false;
	}

	/**
	 * Write a map value.
	 * Will clear the MapBuilder.
	 * The MapBuilder can be re-used to create another array value.
	 */
	void writeMap(MapBuilder &mb);

	/**
	 * Begin writing a map value.
	 * Returns a sub-serializer which map key/value pairs must be written to.
	 * Exactly 'n' keys and 'n' values must be written to the sub-serializer,
	 * and then 'endMap' must be called.
	 */
	Serializer beginMap(size_t n) {
		proceed();
		nesting_ = true;
		nestingLength_ = n * 2;
		writeMapHeader(n);
		return Serializer(w_.os_);
	}

	/**
	 * Complete writing a mwp that was started by 'beginMap'.
	 * The same amount of keys and values must have been written to the serializer
	 * as were passed to the 'beginMap' method.
	 */
	void endMap(Serializer &sub) {
		if (sub.written() != nestingLength_) {
			throw SerializeError("beginMap/endMap length mismatch");
		}

		nesting_ = false;
	}

	/**
	 * Write an extension.
	 */
	void writeExtension(int64_t type, const std::span<unsigned char> ext) {
		// Incrementing written_ will happen in writeInt()
		if (nesting_) {
			throw SerializeError("Missing call to endArray/endMap");
		}

		size_t length = ext.size();
		if (length == 1) {
			w_.writeU8(0xd4);
		} else if (length == 2) {
			w_.writeU8(0xd5);
		} else if (length == 4) {
			w_.writeU8(0xd6);
		} else if (length == 8) {
			w_.writeU8(0xd7);
		} else if (length == 16) {
			w_.writeU8(0xd8);
		} else if (length <= 0xffu) {
			w_.writeU8(0xc7);
			w_.writeU8(length);
		} else if (length <= 0xffffu) {
			w_.writeU8(0xc8);
			w_.writeU16(length);
		} else if (length <= 0xffffffffu) {
			w_.writeU8(0xc9);
			w_.writeU32(length);
		} else {
			throw SerializeError("Extension too long");
		}

		writeInt(type);
		w_.writeBlob(ext.data(), length);
	}

	/**
	 * Get the number of values written to the serializer so far.
	 */
	size_t written() { return written_; }

protected:
	void proceed() {
		if (nesting_) {
			throw SerializeError("Missing call to endArray/endMap");
		}

		written_ += 1;
	}

	void writeArrayHeader(size_t length) {
		if (length <= 0x0fu) {
			w_.writeU8(0x90u | length);
		} else if (length <= 0xffffu) {
			w_.writeU8(0xdc);
			w_.writeU16(length);
		} else if (length <= 0xffffffffu) {
			w_.writeU8(0xdd);
			w_.writeU32(length);
		} else {
			throw SerializeError("Array too long");
		}
	}

	void writeMapHeader(size_t length) {
		if (length <= 0x0fu) {
			w_.writeU8(0x80u | length);
		} else if (length <= 0xffffu) {
			w_.writeU8(0xde);
			w_.writeU16(length);
		} else if (length <= 0xffffffffu) {
			w_.writeU8(0xdf);
			w_.writeU32(length);
		} else {
			throw SerializeError("Array too long");
		}
	}

	detail::Writer w_;
	size_t written_ = 0;
	bool nesting_ = false;
	size_t nestingLength_ = 0;
};

/**
 * A specialized serializer for array values,
 * which writes its values to an internal buffer.
 */
class ArrayBuilder: public Serializer {
public:
	ArrayBuilder(): Serializer(ss_) {}

	/**
	 * Get the underlying buffer.
	 * This moves the buffer out of the underlying string stream.
	 */
	std::string consume() {
		std::string s = std::move(ss_).str();
		ss_ = {};
		written_ = 0;
		return s;
	}

	/**
	 * Set the underlying buffer.
	 * Allows re-using a buffer between array/map builders.
	 * The buffer will be cleared (i.e size() set to 0),
	 * but the capacity will be kept.
	 */
	void setBuffer(std::string &&str) {
		str.clear();
		ss_.str(std::move(str));
	}

private:
	std::stringstream ss_;
};


/**
 * A specialized serializer for map keys and values,
 * which writes its keys and values to an internal buffer.
 */
class MapBuilder: public Serializer {
public:
	MapBuilder(): Serializer(ss_) {}

	/**
	 * Get the underlying buffer.
	 * This moves the buffer out of the underlying string stream.
	 */
	std::string consume() {
		std::string s = std::move(ss_).str();
		ss_ = {};
		written_ = 0;
		return s;
	}

	/**
	 * Set the underlying buffer.
	 * Allows re-using a buffer between array/map builders.
	 * The buffer will be cleared (i.e size() set to 0),
	 * but the capacity will be kept.
	 */
	void setBuffer(std::string &&str) {
		str.clear();
		ss_.str(std::move(str));
	}

private:
	std::stringstream ss_;
};

inline void Serializer::writeArray(ArrayBuilder &ab) {
	writeArrayHeader(ab.written());
	std::string s = ab.consume();
	w_.writeBlob(s.data(), s.size());
	ab.setBuffer(std::move(s));
}

inline void Serializer::writeMap(MapBuilder &mb) {
	if (mb.written() % 2 != 0) {
		throw SerializeError("Odd number of values in map");
	}

	writeMapHeader(mb.written() / 2);
	std::string s = mb.consume();
	w_.writeBlob(s.data(), s.size());
	mb.setBuffer(std::move(s));
}

}
