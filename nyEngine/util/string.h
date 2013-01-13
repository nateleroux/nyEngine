/*
Name: string.h
Author: Nathan LeRoux
Purpose: See string.cpp
*/

#ifndef _STRING_H
#define _STRING_H

class string
{
public:
	string();
	string(const char * source);
	string(const string& source);

	~string();

	string& operator=(const char * other);
	string& operator=(const string& other);
	string& operator=(int other);
	string& operator=(double other);
	string& operator+=(const string& other);
	string& operator+=(const char * other);
	bool operator>(const string& other) const;
	bool operator<(const string& other) const;
	bool operator>=(const string& other) const;
	bool operator<=(const string& other) const;
	bool operator==(const string& other) const;
	bool operator!=(const string& other) const;
	bool operator>(const char * other) const;
	bool operator<(const char * other) const;
	bool operator>=(const char * other) const;
	bool operator<=(const char * other) const;
	bool operator==(const char * other) const;
	bool operator!=(const char * other) const;

	void save(file& f) const;
	void load(file& f);

	int compare(const string& other) const;
	int compare(const char * other) const;

	int asInt() const;
	double asDouble() const;

	const char * c_str() const;
	int length() const;

	uint getHash() const;

private:
	void set(const char * value);
	void resize(int length);

	char * str;
	int strLength;
	int bufferLength;

	uint hashtag;
};

#endif