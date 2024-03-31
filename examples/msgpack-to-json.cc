#include "../msgstream.h"
#include <iostream>
#include <fstream>
#include <span>
#include <string>

static void printString(const std::string &str) {
	std::cout << '"';

	for (char ch: str) {
		if (ch == '\n') {
			std::cout << "\\n";
		} else if (ch == '\r') {
			std::cout << "\\r";
		} else if (ch == '\t') {
			std::cout << "\\t";
		} else if (ch == '"') {
			std::cout << "\\\"";
		} else {
			std::cout << ch;
		}
	}

	std::cout << '"';
}

// JSON doesn't natively support binary and extension types,
// so we'll print them as base64 data URIs
static void printBinary(std::string_view mime, std::span<unsigned char> data) {
	constexpr const char *ALPHABET =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";

	std::string str;
	str += "data:";
	str += mime; 
	str += ";base64,";

	size_t i;
	for (i = 0; i + 2 < data.size(); i += 3) {
		uint32_t num =
			(data[i] << 16) |
			(data[i + 1] << 8) |
			(data[i + 2] << 0);
		str += ALPHABET[(num >> 18) & 0x3f];
		str += ALPHABET[(num >> 12) & 0x3f];
		str += ALPHABET[(num >> 6) & 0x3f];
		str += ALPHABET[(num >> 0) & 0x3f];
	}

	if (data.size() - i == 2) {
		// in:  [ xxxxxxxx yyyyyyyy ]
		// out: [ xxxxxx xxyyyy yyyy00 ]
		uint16_t num = (data[i] << 8) | data[i + 1];
		str += ALPHABET[(num >> 10) & 0x3f];
		str += ALPHABET[(num >> 4) & 0x3f];
		str += ALPHABET[(num << 2) & 0x3f];
		str += '=';
	} else if (data.size() - i == 1) {
		// in:  [ xxxxxxxx ]
		// out: [ xxxxxx xx0000 ]
		uint8_t num = data[i];
		str += ALPHABET[(num >> 2) & 0x3f];
		str += ALPHABET[(num << 4) & 0x3f];
		str += "==";
	}

	printString(str);
}

static void indent(int depth) {
	for (int i = 0; i < depth; ++i) {
		std::cout << "  ";
	}
}

static void printValue(MsgStream::Parser &parser, int depth);

static void printArray(MsgStream::ArrayParser parser, int depth) {
	std::cout << "[\n";

	while (parser.hasNext()) {
		indent(depth + 1);
		printValue(parser, depth + 1);
		if (parser.hasNext()) {
			std::cout << ',';
		}

		std::cout << '\n';
	}

	indent(depth);
	std::cout << ']';
}

static void printMap(MsgStream::MapParser parser, int depth) {
	std::cout << "{\n";

	while (parser.hasNext()) {
		indent(depth + 1);

		printValue(parser, depth + 1);

		std::cout << ": ";

		printValue(parser, depth + 1);

		if (parser.hasNext()) {
			std::cout << ',';
		}

		std::cout << '\n';
	}

	indent(depth);
	std::cout << '}';
}

static void printValue(MsgStream::Parser &parser, int depth) {
	using Type = MsgStream::Type;

	switch (parser.nextType()) {
	case Type::INT:
		std::cout << parser.nextInt();
		break;
	case Type::UINT:
		std::cout << parser.nextUInt();
		break;
	case Type::NIL:
		parser.skipNil();
		std::cout << "null";
		break;
	case Type::BOOL:
		std::cout << (parser.nextBool() ? "true" : "false");
		break;
	case Type::FLOAT32:
		std::cout << parser.nextFloat32();
		break;
	case Type::FLOAT64:
		std::cout << parser.nextFloat64();
		break;
	case Type::STRING:
		printString(parser.nextString());
		break;
	case Type::BINARY: {
		auto bin = parser.nextBinary();
		printBinary("application/octet-stream", bin);
	}
		break;
	case Type::ARRAY:
		printArray(parser.nextArray(), depth);
		break;
	case Type::MAP:
		printMap(parser.nextMap(), depth);
		break;
	case Type::EXTENSION: {
		std::vector<unsigned char> bin;
		int64_t type = parser.nextExtension(bin);
		std::string mime = "application/x-msgpack-ext.";
		mime += std::to_string(type);
		printBinary(mime, bin);
	}
		break;
	}
}

int main(int argc, char **argv) {
	std::ifstream file;
	std::istream *is;
	if (argc == 1) {
		is = &std::cin;
	} else if (argc == 2) {
		file = std::ifstream(argv[1]);
		if (!file) {
			std::cerr << "Failed to open " << argv[1] << '\n';
			return 1;
		}
		is = &file;
	} else {
		std::cerr << "Usage: " << argv[0] << " [file]\n";
		return 1;
	}

	try {
		MsgStream::Parser parser(*is);

		while (parser.hasNext()) {
			printValue(parser, 0);
			std::cout << '\n';
		}
	} catch (MsgStream::ParseError &err) {
		std::cerr << "Parse error: " << err.what() << '\n';
		return 1;
	}
}
