#include "../msgstream.h"
#include <iostream>
#include <fstream>

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
	case Type::FLOAT:
		std::cout << parser.nextFloat();
		break;
	case Type::STRING:
		printString(parser.nextString());
		break;
	case Type::BINARY:
		parser.skipNext();
		std::cout << "(binary)";
		break;
	case Type::ARRAY:
		printArray(parser.nextArray(), depth);
		break;
	case Type::MAP:
		printMap(parser.nextMap(), depth);
		break;
	case Type::EXTENSION:
		parser.skipNext();
		std::cout << "(extension)";
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

	MsgStream::Parser parser(*is);

	while (parser.hasNext()) {
		printValue(parser, 0);
		std::cout << '\n';
	}
}
