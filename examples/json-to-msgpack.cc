#include "../msgstream.h"
#include <json/json.h>
#include <iostream>
#include <fstream>

void writeValue(MsgStream::Serializer &serializer, const Json::Value &val) {
	if (val.isUInt() || val.isUInt64()) {
		serializer.writeUInt(val.asUInt64());
	} else if (val.isInt() || val.isInt64()) {
		serializer.writeInt(val.asInt64());
	} else if (val.isNumeric()) {
		serializer.writeFloat64(val.asDouble());
	} else if (val.isNull()) {
		serializer.writeNil();
	} else if (val.isBool()) {
		serializer.writeBool(val.asBool());
	} else if (val.isString()) {
		serializer.writeString(val.asCString());
	} else if (val.isArray()) {
		auto sub = serializer.beginArray(val.size());
		for (Json::ArrayIndex i = 0; i < val.size(); ++i) {
			writeValue(sub, val[i]);
		}
	} else if (val.isObject()) {
		auto keys = val.getMemberNames();
		auto sub = serializer.beginMap(keys.size());
		for (auto &key: keys) {
			sub.writeString(key);
			writeValue(sub, val[key]);
		}
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

	Json::Value val;
	*is >> val;

	MsgStream::Serializer serializer(std::cout);
	writeValue(serializer, val);
}
