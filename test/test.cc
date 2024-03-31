#include "../msgstream.h"
#include <sstream>
#include <stdexcept>
#include <stdint.h>
#include <json/json.h>
#include <iostream>
#include <fstream>
#include <unordered_set>

struct Stats {
	int numTotalTests = 0;
	int numPassedTests = 0;
	int numTotalChecks = 0;
	int numPassedChecks = 0;
};

static uint8_t decodeHexChar(char hex) {
	if (hex >= '0' && hex <= '9') {
		return hex - '0';
	} else if (hex >= 'a' && hex <= 'f') {
		return hex - 'a' + 10;
	} else if (hex >= 'A' && hex <= 'F') {
		return hex - 'A' + 10;
	} else {
		throw std::runtime_error("Invalid hex digit");
	}
}

static char encodeHexChar(uint8_t nibble) {
	if (nibble <= 9) {
		return '0' + nibble;
	} else if (nibble <= 16) {
		return 'A' + (nibble - 10);
	} else {
		throw std::runtime_error("Hex nibble out of range");
	}
}

static std::string hexToBytes(const std::string_view hexStr) {
	std::string binStr;
	auto it = hexStr.begin();
	auto end = hexStr.end();
	while (it != end) {
		if (*it == '-') {
			it++;
			continue;
		}

		char hi = decodeHexChar(*(it++));
		if (it == end) {
			throw std::runtime_error("Unexpected EOF");
		}

		char lo = decodeHexChar(*(it++));

		binStr.push_back((hi << 4) | lo);
	}

	return binStr;
}

static std::string bytesToHex(const std::string_view bin) {
	std::string hex;
	for (size_t i = 0; i < bin.size(); ++i) {
		hex += encodeHexChar(((uint8_t)bin[i]) >> 4);
		hex += encodeHexChar(((uint8_t)bin[i]) & 0x0f);
		if (i != bin.size() - 1) {
			hex += '-';
		}
	}

	return hex;
}

template<typename T>
static void assertEqual(const T &actual, const T &expected, const char *msg) {
	if (actual == expected) {
		return;
	}

	std::stringstream ss;
	ss << msg << ": Expected '" << expected << "', got '" << actual << '\'';
	throw std::runtime_error(ss.str());
}

static void assertArraysEqual(MsgStream::ArrayParser parser, Json::Value &arr);

static void assertMapsEqual(MsgStream::MapParser parser, Json::Value &arr);

static void assertValuesEqual(MsgStream::Parser &parser, Json::Value &val) {
	using Type = MsgStream::Type;
	switch (parser.nextType()) {
	case Type::INT:
		assertEqual(parser.nextInt(), val.asInt64(), "Invalid int value");
		break;
	case Type::UINT:
		assertEqual(parser.nextUInt(), val.asUInt64(), "Invalid uint value");
		break;
	case Type::NIL:
		if (!val.isNull()) {
			throw std::runtime_error("Invalid value: Expected non-null");
		}
		parser.skipNil();
		break;
	case Type::BOOL:
		assertEqual(parser.nextBool(), val.asBool(), "Invalid value");
		break;
	case Type::FLOAT:
		assertEqual(parser.nextFloat(), val.asDouble(), "Invalid value");
		break;
	case Type::STRING:
		assertEqual(parser.nextString(), val.asString(), "Invalid value");
		break;
	case Type::BINARY:
		throw std::runtime_error(
			"Invalid value: Got binary when comparing against JSON object");
		break;
	case Type::ARRAY:
		assertArraysEqual(parser.nextArray(), val);
		break;
	case Type::MAP:
		assertMapsEqual(parser.nextMap(), val);
		break;
	case Type::EXTENSION:
		throw std::runtime_error(
			"Invalid value: Got extension when comparing against JSON object");
		break;
	}
}

static void assertArraysEqual(MsgStream::ArrayParser parser, Json::Value &arr) {
	if (!arr.isArray()) {
		throw std::runtime_error("Invalid value: Expected non-array");
	}

	Json::ArrayIndex index = 0;
	while (parser.hasNext()) {
		if (index >= arr.size()) {
			throw std::runtime_error("Invalid value: Expected shorter array");
		}

		assertValuesEqual(parser, arr[index]);
		index += 1;
	}
}

static void assertMapsEqual(MsgStream::MapParser parser, Json::Value &obj) {
	if (!obj.isObject()) {
		throw std::runtime_error("Invalid value: Expected non-object");
	}

	std::unordered_set<std::string> keys;
	while (parser.hasNext()) {
		auto key = parser.nextString();
		if (!obj.isMember(key)) {
			throw std::runtime_error(
				"Invalid value: Unexpected object key '" + key + "'");
		}

		if (keys.contains(key)) {
			throw std::runtime_error("Invalid value: Duplicate key '" + key + "'");
		}

		keys.insert(key);
		assertValuesEqual(parser, obj[key]);
	}

	for (auto &key: obj.getMemberNames()) {
		if (!keys.contains(key)) {
			throw std::runtime_error("Invalid value: Missing key '" + key + "'");
		}
	}
}

static void check(std::string bin, Json::Value &val, Stats &stats) {
	stats.numTotalChecks += 1;

	std::stringstream ss(std::move(bin));
	MsgStream::Parser parser(ss);

	auto assertNotDone = [&] {
		if (!parser.hasNext()) {
			throw std::runtime_error("Parser doesn't have enough values");
		}
	};

	auto assertIsDone = [&] {
		if (parser.hasNext()) {
			throw std::runtime_error(
				std::string("There's trailing garbage: ") +
				encodeHexChar(ss.peek() >> 4) +
				encodeHexChar(ss.peek() & 0x0f));
		}
	};

	assertNotDone();

	if (val.isMember("nil")) {
		parser.skipNil();
	} else if (val.isMember("bool")) {
		assertEqual(
			parser.nextBool(), val["bool"].asBool(),
			"Incorrect boolean value");
	} else if (val.isMember("binary")) {
		auto expected = hexToBytes(val["binary"].asCString());
		auto actual = parser.nextBinary();
		assertEqual(
			actual.size(), expected.size(),
			"Incorrect binary: sizes differ");

		for (size_t i = 0; i < actual.size(); ++i) {
			if (actual[i] != (unsigned char)expected[i]) {
				throw std::runtime_error("Incorrect binary: values differ");
			}
		}
	} else if (val.isMember("number") || val.isMember("bignum")) {
		Json::Value num;
		if (val.isMember("number")) {
			num = val["number"];
		} else {
			std::stringstream{val["bignum"].asString()} >> num;
		}

		auto type = parser.nextType();
		if (type == MsgStream::Type::INT) {
			assertEqual(
				parser.nextInt(), num.asInt64(),
				"Incorrect int value");
		} else if (type == MsgStream::Type::UINT) {
			assertEqual(
				parser.nextUInt(), num.asUInt64(),
				"Incorrect uint value");
		} else if (type == MsgStream::Type::FLOAT) {
			assertEqual(
				parser.nextFloat(), num.asDouble(),
				"Incorrect float value");
		} else {
			throw std::runtime_error("Value not number");
		}
	} else if (val.isMember("string")) {
		assertEqual<std::string_view>(
			parser.nextString(), val["string"].asCString(),
			"Incorrect string value");
	} else if (val.isMember("array")) {
		assertArraysEqual(parser.nextArray(), val["array"]);
	} else if (val.isMember("map")) {
		assertMapsEqual(parser.nextMap(), val["map"]);
	} else if (val.isMember("ext")) {
		auto expected = hexToBytes(val["ext"][1].asCString());
		int64_t expectedType = val["ext"][0].asInt64();
		std::vector<unsigned char> actual;
		int64_t actualType = parser.nextExtension(actual);
		assertEqual(
			actualType, expectedType,
			"Incorrect extension: types differ");
		assertEqual(
			actual.size(), expected.size(),
			"Incorrect extension: sizes differ");

		for (size_t i = 0; i < actual.size(); ++i) {
			if ((unsigned char)expected[i] != actual[i]) {
				throw std::runtime_error("Incorrect extension: values differ");
			}
		}
	} else {
		std::stringstream ss;
		ss << val;
		throw std::runtime_error("Invalid JSON value: " + std::move(ss).str());
	}

	assertIsDone();

	stats.numPassedChecks += 1;
}

static void roundtripValue(MsgStream::Parser &i, MsgStream::Serializer &o) {
	using Type = MsgStream::Type;
	switch (i.nextType()) {
	case Type::INT:
		o.writeInt(i.nextInt());
		break;
	case Type::UINT:
		o.writeUInt(i.nextUInt());
		break;
	case Type::NIL:
		i.skipNil();
		o.writeNil();
		break;
	case Type::BOOL:
		o.writeBool(i.nextBool());
		break;
	case Type::FLOAT:
		o.writeFloat64(i.nextFloat());
		break;
	case Type::STRING:
		o.writeString(i.nextString());
		break;
	case Type::BINARY:
		o.writeBinary(i.nextBinary());
		break;
	case Type::ARRAY: {
		auto ai = i.nextArray();
		auto ao = o.beginArray(ai.arraySize());
		while (ai.hasNext()) {
			roundtripValue(ai, ao);
		}
		o.endArray(ao);
	}
		break;
	case Type::MAP: {
		auto mi = i.nextMap();
		auto mo = o.beginMap(mi.mapSize());
		std::string key;
		while (mi.hasNext()) {
			mi.nextString(key);
			mo.writeString(key);
			roundtripValue(mi, mo);
		}
		o.endMap(mo);
	}
		break;
	case Type::EXTENSION: {
		std::vector<unsigned char> ext;
		int64_t type = i.nextExtension(ext);
		o.writeExtension(type, ext);
	}
		break;
	}
}

static std::string roundtrip(std::string bin) {
	std::stringstream is(std::move(bin));
	std::stringstream os;

	MsgStream::Parser parser(is);
	MsgStream::Serializer serializer(os);

	while (parser.hasNext()) {
		roundtripValue(parser, serializer);
	}

	return std::move(os).str();
}

static void runTest(Json::Value &val, Stats &stats) {
	stats.numTotalTests += 1;

	auto &msgpacks = val["msgpack"];
	if (!msgpacks.isArray()) {
		std::cout << "FAIL! Key 'msgpack' is not an array\n";
		return;
	}

	for (Json::ArrayIndex i = 0; i < msgpacks.size(); ++i) {
		auto &msgpackHex = msgpacks[i];
		std::string bin = hexToBytes(msgpackHex.asCString());

		try {
			check(bin, val, stats);
		} catch (std::exception &ex) {
			std::cout
				<< "FAIL! Check " << (i + 1) << '/' << msgpacks.size() << '\n'
				<< "   -- Err: " << ex.what() << '\n'
				<< "   -- msgpack: " << bytesToHex(bin) << '\n'
				<< '\n';
			return;
		}

		std::string roundtripped;
		try {
			roundtripped = roundtrip(bin);
		} catch (std::exception &ex) {
			std::cout
				<< "FAIL! Check " << (i + 1) << '/' << msgpacks.size() << '\n'
				<< "   -- Roundtrip err: " << ex.what() << '\n'
				<< "   -- msgpack: " << bytesToHex(bin) << '\n'
				<< '\n';
			return;
		}

		try {
			check(roundtripped, val, stats);
		} catch (std::exception &ex) {
			std::cout
				<< "FAIL! Check " << (i + 1) << '/' << msgpacks.size()
				<< " (roundtripped)\n"
				<< "   -- Err: " << ex.what() << '\n'
				<< "   -- Old msgpack: " << bytesToHex(bin) << '\n'
				<< "   -- New msgpack: " << bytesToHex(roundtripped) << '\n'
				<< '\n';
			return;
		}

	}

	std::cout << "OK!\n";
	stats.numPassedTests += 1;
	return;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <test json>\n";
		return 1;
	}

	std::ifstream is(argv[1]);
	if (!is) {
		std::cerr << "Failed to open " << argv[1] << '\n';
		return 1;
	}

	Json::Value groups;
	is >> groups;
	is.close();

	Stats stats;

	auto groupNames = groups.getMemberNames();
	for (size_t groupIndex = 0; groupIndex < groupNames.size(); ++groupIndex) {
		auto &groupName = groupNames[groupIndex];
		auto &group = groups[groupName];
		std::cout << groupName << ":\n";

		Json::ArrayIndex testIndex;
		for (testIndex = 0; testIndex < group.size(); ++testIndex) {
			auto &test = group[testIndex];

			// We don't have special timestamp handling,
			// so just skip tests which deal with timestamps
			if (test.isMember("timestamp")) {
				continue;
			}

			if (testIndex < 9) {
				std::cout << ' ';
			}

			std::cout <<
				"  " << (testIndex + 1) << '/' << group.size()
				<< ": " << std::flush;

			runTest(test, stats);
		}
	}

	std::cout
		<< '\n'
		<< "Tests passed: "
		<< stats.numPassedTests << '/' << stats.numTotalTests
		<< '\n'
		<< "Checks passed: "
		<< stats.numPassedChecks << '/' << stats.numTotalChecks
		<< "\n\n";
	if (stats.numPassedTests == stats.numTotalTests) {
		std::cout << "Success!\n";
		return 0;
	} else {
		std::cout << "Failure!\n";
		return 1;
	}
}
