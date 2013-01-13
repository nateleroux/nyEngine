/*
Name: file.h
Author: Nathan LeRoux
Purpose: See file.cpp
*/

#ifndef _FILE_H
#define _FILE_H

class file
{
public:
	file();
	~file();

	// Open a file for reading or writing
	bool openRead(const char * path, bool binary = true);
	bool openWrite(const char * path, bool binary = true, bool append = false);

	// Close the file
	void close();

	// Set the pointer (only up to 4 GB)
	void seek(uint offset);
	// Get the pointer
	uint offset();

	// Get the file size
	uint size();

	// Write to the file
	void write(double x);
	void write(uint x);
	void write(uint16 x);
	void write(uint8 x);
	void write(int x);
	void write(int16 x);
	void write(int8 x);
	void write(const void * data, int length);

	// Read from the file
	double readdouble();
	uint readuint32();
	uint16 readuint16();
	uint8 readuint8();
	int readint32();
	int16 readint16();
	int8 readint8();
	void read(void * data, int length);

	// Get the current checksum value
	uint getChecksum();

private:
	void updateChecksum(const void * data, int length);
	void addChecksum(uint8 x);

	uint checksum, checksumBuffer, checksumIndex;
	FILE * rawfile;
};

#endif