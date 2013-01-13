/*
Name: file.cpp
Author: Nathan LeRoux
Purpose: File reading and writing
*/

#include "..\include.h"

file::file()
{
	checksum = 0;
	checksumIndex = 0;
	checksumBuffer = 0;
	rawfile = NULL;
}

file::~file()
{
	close();
}

bool file::openRead(const char * path, bool binary)
{
	rawfile = fopen(path, binary ? "rb" : "r");
	return rawfile != NULL;
}

bool file::openWrite(const char * path, bool binary, bool append)
{
	// Build the mode string
	char mode[3];

	if(append)
		mode[0] = 'a';
	else
		mode[0] = 'w';

	if(binary)
	{
		mode[1] = 'b';
		mode[2] = 0;
	}
	else
		mode[1] = 0;

	rawfile = fopen(path, mode);
	return rawfile != NULL;
}

void file::close()
{
	if(rawfile)
	{
		fclose(rawfile);
		checksum = 0;
		checksumIndex = 0;
		checksumBuffer = 0;
		rawfile = NULL;
	}
}

void file::seek(uint offset)
{
	fseek(rawfile, offset, SEEK_SET);
}

uint file::offset()
{
	return ftell(rawfile); 
}

uint file::size()
{
	uint end;
	uint off = offset();
	fseek(rawfile, 0, SEEK_END);
	end = offset();
	seek(off);
	return end;
}

void file::write(double x)
{
	write(&x, sizeof(x));
}

void file::write(uint x)
{
	write(&x, sizeof(x));
}

void file::write(uint16 x)
{
	write(&x, sizeof(x));
}

void file::write(uint8 x)
{
	write(&x, sizeof(x));
}

void file::write(int x)
{
	write(&x, sizeof(x));
}

void file::write(int16 x)
{
	write(&x, sizeof(x));
}

void file::write(int8 x)
{
	write(&x, sizeof(x));
}

void file::write(const void * data, int length)
{
	int l;

	if(rawfile == NULL)
		dbgError("file handle invalid");

	l = fwrite(data, length, 1, rawfile);
	updateChecksum(data, length);

	if(length && l != 1)
		dbgError("unable to write to file");
}

double file::readdouble()
{
	double x;
	read(&x, sizeof(x));

	return x;
}

uint file::readuint32()
{
	uint x;
	read(&x, sizeof(x));

	return x;
}

uint16 file::readuint16()
{
	uint16 x;
	read(&x, sizeof(x));

	return x;
}

uint8 file::readuint8()
{
	uint8 x;
	read(&x, sizeof(x));

	return x;
}

int file::readint32()
{
	int x;
	read(&x, sizeof(x));

	return x;
}

int16 file::readint16()
{
	int16 x;
	read(&x, sizeof(x));

	return x;
}

int8 file::readint8()
{
	int8 x;
	read(&x, sizeof(x));

	return x;
}

void file::read(void * data, int length)
{
	int l;

	if(rawfile == NULL)
		dbgError("file handle invalid");

	l = fread(data, length, 1, rawfile);
	updateChecksum(data, length);

	if(length && l != 1)
		dbgError("unable to read from file");
}

uint file::getChecksum()
{
	uint i;

	// Finalize the checksum
	if(checksumIndex != 0)
	{
		for(i = 4;i > checksumIndex;i--)
		{
			addChecksum(0);
		}
	}

	return checksum;
}

void file::updateChecksum(const void * data, int length)
{
	int i;
	for(i = 0;i < length;i++)
		addChecksum(((uint8*)data)[i]);
}

void file::addChecksum(uint8 x)
{
	int magic = 0x0020212F; // Nonsense number
	// The bits of this number are fibbonacci numbers
	// 0, 1, 2, 3, 5, 8, 13, 21

	// Update a single byte into the checksum buffer
	checksumBuffer |= x << (8 * checksumIndex);

	if(checksumIndex == 3)
	{
		checksumIndex = 0;

		// Process this into the main sum
		checksum ^= checksumBuffer;
		checksum += magic;
	}
	else
		checksumIndex++;
}
