#ifndef __SPARCEL_H__
#define __SPARCEL_H__

#include <string>
#include <map>

typedef std::map<unsigned int, unsigned char> ByteMap;
typedef std::map<unsigned int, int>           IntMap;
typedef std::map<unsigned int, long long>     LongLongMap;
typedef std::map<unsigned int, std::string>   StringMap;
typedef std::map<unsigned int, void *>        PointerMap;

class SParcel {
public:
	void setByte(unsigned int key, unsigned char value);
	void setInt(unsigned int key, int value);
	void setLlong(unsigned int key, long long value);
	void setString(unsigned int key, const char *value);
	void setPointer(unsigned int key, void *value);

	bool findByte(unsigned int key, unsigned char *value);
	bool findInt(unsigned int key, int *value);
	bool findLlong(unsigned int key, long long *value);
	bool findString(unsigned int key, std::string *value);
	bool findPointer(unsigned int key, void **value);

	bool eraseByte(unsigned int key);
	bool eraseInt(unsigned int key);
	bool eraseLlong(unsigned int key);
	bool eraseString(unsigned int key);
	bool erasePointer(unsigned int key);

	// NOTE: The default assignment operator will be used
	// SParcel& operator=(const SParcel&);

private:
	ByteMap mByte;
	IntMap mInt;
	LongLongMap mLlong;
	StringMap mString;
	PointerMap mPointer;
};

#endif // __SPARCEL_H__

