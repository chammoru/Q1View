#include "SParcel.h"

using namespace std;

void SParcel::setByte(unsigned int key, unsigned char value) {
	mByte[key] = value;
}

void SParcel::setInt(unsigned int key, int value) {
	mInt[key] = value;
}

void SParcel::setLlong(unsigned int key, long long value) {
	mLlong[key] = value;
}

void SParcel::setString(unsigned int key, const char *value) {
	mString[key] = string(value);
}

void SParcel::setPointer(unsigned int key, void *value) {
	mPointer[key] = value;
}

bool SParcel::findByte(unsigned int key, unsigned char *value) {
	bool ret = true;
	ByteMap::iterator it;

	it = mByte.find(key);
	if (it != mByte.end()) {
		*value = it->second;
	} else {
		*value = 0;
		ret = false;
	}

	return ret;
}

bool SParcel::findInt(unsigned int key, int *value) {
	bool ret = true;
	IntMap::iterator it;

	it = mInt.find(key);
	if (it != mInt.end()) {
		*value = it->second;
	} else {
		*value = 0;
		ret = false;
	}

	return ret;
}

bool SParcel::findLlong(unsigned int key, long long *value) {
	bool ret = true;
	LongLongMap::iterator it;

	it = mLlong.find(key);
	if (it != mLlong.end()) {
		*value = it->second;
	} else {
		*value = 0;
		ret = false;
	}

	return ret;
}

bool SParcel::findString(unsigned int key, string *value) {
	bool ret = true;
	StringMap::iterator it;

	it = mString.find(key);
	if (it != mString.end()) {
		*value = it->second;
	} else {
		*value = "";
		ret = false;
	}

	return ret;
}

bool SParcel::findPointer(unsigned int key, void **value) {
	bool ret = true;
	PointerMap::iterator it;

	it = mPointer.find(key);
	if (it != mPointer.end()) {
		*value = it->second;
	} else {
		*value = 0;
		ret = false;
	}

	return ret;
}

bool SParcel::eraseByte(unsigned int key) {
	bool ret = true;
	ByteMap::iterator it;

	it = mByte.find(key);
	if (it != mByte.end()) {
		mByte.erase(it);
	} else {
		ret = false;
	}

	return ret;
}

bool SParcel::eraseInt(unsigned int key) {
	bool ret = true;
	IntMap::iterator it;

	it = mInt.find(key);
	if (it != mInt.end()) {
		mInt.erase(it);
	} else {
		ret = false;
	}

	return ret;
}

bool SParcel::eraseLlong(unsigned int key) {
	bool ret = true;
	LongLongMap::iterator it;

	it = mLlong.find(key);
	if (it != mLlong.end()) {
		mLlong.erase(it);
	} else {
		ret = false;
	}

	return ret;
}

bool SParcel::eraseString(unsigned int key) {
	bool ret = true;
	StringMap::iterator it;

	it = mString.find(key);
	if (it != mString.end()) {
		mString.erase(it);
	} else {
		ret = false;
	}

	return ret;
}

bool SParcel::erasePointer(unsigned int key) {
	bool ret = true;
	PointerMap::iterator it;

	it = mPointer.find(key);
	if (it != mPointer.end()) {
		mPointer.erase(it);
	} else {
		ret = false;
	}

	return ret;
}

