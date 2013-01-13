/*
Name: string.cpp
Author: Nathan LeRoux
Purpose: String manipulation
*/

#include "..\include.h"

string::string()
{
	str = NULL;
	strLength = 0;
	bufferLength = 0;
}

string::string(const char * source)
{
	str = NULL;
	strLength = 0;
	bufferLength = 0;

	set(source);
}

string::string(const string& source)
{
	str = NULL;
	strLength = 0;
	bufferLength = 0;

	set(source.c_str());
}

string::~string()
{
	if(str != NULL)
		free(str);
}

string& string::operator=(const char * other)
{
	set(other);

	return *this;
}

string& string::operator=(const string& other)
{
	set(other.c_str());

	return *this;
}

string& string::operator=(int other)
{
	char buffer[20];
	
	_itoa(other, buffer, 10);
	set(buffer);

	return *this;
}

string& string::operator=(double other)
{
	char buffer[40];
	int i, len;
	
	len = _snprintf(buffer, sizeof(buffer), "%Lf", other);

	// Remove any trailing zeros
	if(strchr(buffer, '.'))
	{
		for(i = len - 1;i >= 0;i--)
		{
			if(buffer[i] == '.')
			{
				buffer[i] = 0;
				break;
			}
			else if(buffer[i] != '0')
				break;

			buffer[i] = 0;
		}
	}

	set(buffer);
	
	return *this;
}

string& string::operator+=(const string& other)
{
	resize(strLength + other.length() + 1);
	strcat(str, other.str);
	set(str);
	return *this;
}

string& string::operator+=(const char * other)
{
	resize(strLength + strlen(other) + 1);
	strcat(str, other);
	set(str);
	return *this;
}

bool string::operator>(const string& other) const
{
	return compare(other) > 0;
}

bool string::operator<(const string& other) const
{
	return compare(other) < 0;
}

bool string::operator>=(const string& other) const
{
	return compare(other) > 0;
}

bool string::operator<=(const string& other) const
{
	return compare(other) < 0;
}

bool string::operator==(const string& other) const
{
	if(other.getHash() != hashtag)
		return false;

	return compare(other) == 0;
}

bool string::operator!=(const string& other) const
{
	if(other.getHash() != hashtag)
		return true;

	return compare(other) != 0;
}

bool string::operator>(const char * other) const
{
	return compare(other) > 0;
}

bool string::operator<(const char * other) const
{
	return compare(other) < 0;
}

bool string::operator>=(const char * other) const
{
	return compare(other) > 0;
}

bool string::operator<=(const char * other) const
{
	return compare(other) < 0;
}

bool string::operator==(const char * other) const
{
	return compare(other) == 0;
}

bool string::operator!=(const char * other) const
{
	return compare(other) != 0;
}

void string::save(file& f) const
{
	// Write the length, followed by the string
	// If the length is >= 0xFF, then write 0xFF followed by the length as a short
	// If the length is > 0xFFFF, then error
	if(strLength > 0xFFFF)
		dbgError("string is too long");

	if(strLength < 0xFF)
		f.write((byte)strLength);
	else
	{
		f.write((byte)0xFF);
		f.write((ushort)strLength);
	}

	f.write(str, strLength);
}

void string::load(file& f)
{
	// Set the buffer and read in the string
	int len = f.readuint8();
	if(len == 0xFF)
		len = f.readuint16();

	resize(len + 1);
	strLength = bufferLength - 1;
	f.read(str, strLength);

	// Null terminate the string
	str[strLength] = 0;

	// Update the hashtag
	set(str);
}

int string::compare(const string& other) const
{
	return _stricmp(str, other.c_str());
}

int string::compare(const char * other) const
{
	return _stricmp(str, other);
}

int string::asInt() const
{
	if(*this == "true")
		return 1;
	else if(*this == "false")
		return 0;
	else
		return atoi(str);
}

double string::asDouble() const
{
	if(*this == "true")
		return 1;
	else if(*this == "false")
		return 0;
	else
		return atof(str);
}

const char * string::c_str() const
{
	return str;
}

int string::length() const
{
	return strLength;
}

uint string::getHash() const
{
	return hashtag;
}

void string::set(const char * value)
{
	int i;
	char c;
	int l = strlen(value) + 1;

	if(bufferLength != l)
		resize(l);

	hashtag = 0;
	for(i = 0;i < l;i++)
	{
		c = tolower(value[i]);
		str[i] = value[i];
		hashtag += (0x0020212F) ^ (c << 24 | c << 16 | c << 8 | c);
	}

	strLength = l - 1;
}

void string::resize(int length)
{
	if(str == NULL)
		str = (char *)malloc(length);
	else
		str = (char *)realloc(str, length);

	bufferLength = length;
}