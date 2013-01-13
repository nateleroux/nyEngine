/*
Name: map.cpp
Author: Nathan LeRoux
Purpose: Process resource archive files
*/

#include "..\include.h"
#include "..\util\LuaManager.h"
#include <io.h>
#include <vector>
#include <../zlib.h>

#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32

// Strip this code from release builds
#ifdef _DEBUG
void mapGatherDirectory(string& dir, string& prefix, int dirClip, std::vector<string>& files)
{
	intptr_t find;
	_finddata32_t data;
	string nextdir, nextpre;
	string search = dir;
	
	search += "/*";
	find = _findfirst32(search.c_str(), &data);

	if(find == -1)
		return;

	do
	{
		if(_stricmp(data.name, ".") == 0 || _stricmp(data.name, "..") == 0)
			continue; // We dont want these in here...

		if(data.attrib & _A_SUBDIR)
		{
			nextdir = dir;
			nextdir += "/";
			nextdir += data.name;
			nextpre = prefix;
			nextpre += "/";
			nextpre += data.name;
			
			mapGatherDirectory(nextdir, nextpre, dirClip, files);
		}
		else if(!(data.attrib & _A_HIDDEN))
		{
			// Found a file
			string name;
			name = dir.c_str() + dirClip;
			name += "/";
			name += data.name;
			files.push_back(name);
		}
	} while(_findnext32(find, &data) == 0);

	_findclose(find);
}

void createDir(const char * dirname)
{
	char dirPath[0x200];
	const char * c = dirname;

	memset(dirPath, 0, sizeof(dirPath));

	while(true)
	{
		c = strchr(c, '/') + 1;
		if(c == (char*)1)
			break;

		strncpy(dirPath, dirname, c - dirname - 1);
		CreateDirectory(dirPath, NULL);
	}
}

void mapCompileLua(const char * filename, const char * outname)
{
	file in, out;
	char dirPath[0x200];

	memset(dirPath, 0, sizeof(dirPath));
	strncpy(dirPath, outname, strrchr(outname, '/') - outname);
	createDir(outname);

	if(!in.openRead(filename))
		dbgError("unable to open '%s' for reading", filename);
	if(!out.openWrite(outname))
		dbgError("unable to open '%s' for writing", outname);

	CLuaManager::Compile(in, out, filename);

	in.close();
	out.close();
}

void mapCompile(const char * filename, const char * path, const char * prefix)
{
	uint i, j, k, l;
	char buffer[(1024 * 3) / 2];
	char compBuffer[1024 * 3];
	uint readLen, writeLen;
	file map, item;
	string dir = path, pre = prefix, name;
	std::vector<string> files;
	std::vector<int> types;
	z_stream str;
	int typeCounts[MSectionCount] = {0};
	const char * extensions[] =
	{
		(const char *)1,
		"zone",
		(const char *)1,
		"mesh",
		(const char *)1,
		"texture",
		(const char *)1,
		"material",
		(const char *)1,
		"shader",
		(const char *)1,
		"ogg",
		(const char *)1,
		"ent",
		(const char *)1,
		"lua",
		(const char *)2,
		"oscript",
		"widget",
		(const char *)0,
	};

	// Gather all files in the directory
	mapGatherDirectory(dir, pre, dir.length(), files);
	
	// Scan the extensions
	for(i = 0;i < files.size();i++)
	{
		int fileType = -1;
		const char * ext = strrchr(files[i].c_str(), '.') + 1;
		if(ext)
		{
			for(j = 0,l = -1,k = 0;j < sizeof(extensions) / sizeof(char*);j++)
			{
				if(k == 0)
				{
					k = (int)extensions[j];
					l++;

					continue;
				}

				if(_stricmp(ext, extensions[j]) == 0)
				{
					fileType = l;
					break;
				}

				k--;
			}
		}

		if(fileType == -1)
			fileType = MSectionGeneric;

		types.push_back(fileType);
		typeCounts[fileType]++;
	}

	// Build the map
	if(!map.openWrite(filename))
		dbgError("unable to open map for writing");

	// Header
	map.write((uint)MAP_MAGIC);
	map.write((ushort)MAP_MAJOR);
	map.write((ushort)MAP_MINOR);
	map.write((uint)0); // TODO: map flags

	name = filename;
	name.save(map);

	// Sections
	for(i = 0;i < MSectionCount;i++)
	{
		uint sizeOffset;
		uint sectionSize;

		sizeOffset = map.offset();
		map.write((uint)0);
		map.write((uint)typeCounts[i]);

		// Items
		for(j = 0;j < files.size();j++)
		{
			uint compressedSize = 0;
			uint size, compressedSizeOffset;

			if(types[j] != i)
				continue;
			
			if(i == MSectionScript)
			{
				// Lua script is special and must be compiled first
				name = "compile/";
				name += path;
				name += files[j];

				string nameTemp = path;
				nameTemp += files[j];
				mapCompileLua(nameTemp.c_str(), name.c_str());
			}
			else
			{
				name = path;
				name += files[j];
			}

			if(!item.openRead(name.c_str()))
				dbgError("unable to open file '%s'", files[j].c_str());

			name = prefix;
			name += files[j];

			size = item.size();

			map.write(size);
			compressedSizeOffset = map.offset();
			map.write((uint)0);

			name.save(map);

			memset(&str, 0, sizeof(str));
			if(deflateInit(&str, 7) < Z_OK)
				dbgError("deflateInit failed");
			
			while(size)
			{
				readLen = sizeof(buffer);

				if(readLen > size)
					readLen = size;

				writeLen = readLen;

				size -= readLen;

				item.read(buffer, readLen);
				str.next_in = (Bytef*)buffer;
				str.avail_in = readLen;
				str.next_out = (Bytef*)compBuffer;
				str.avail_out = sizeof(compBuffer);

				while(str.avail_in)
				{
					if(deflate(&str, Z_NO_FLUSH) < Z_OK)
						dbgError("deflate failed");

					writeLen = sizeof(compBuffer) - str.avail_out;
					if(writeLen)
					{
						map.write(compBuffer, writeLen);
						str.next_out = (Bytef*)compBuffer;
						str.avail_out = sizeof(compBuffer);
						compressedSize += writeLen;
					}
				}
			}

			if(item.size())
			{
				int err;
				do
				{
					err = deflate(&str, Z_FINISH);
					if(err < Z_OK)
						dbgError("failed to deflate");

					writeLen = sizeof(compBuffer) - str.avail_out;
					if(writeLen)
					{
						map.write(compBuffer, writeLen);
						str.next_out = (Bytef*)compBuffer;
						str.avail_out = sizeof(compBuffer);
						compressedSize += writeLen;
					}
				} while(err != Z_STREAM_END);

				deflateEnd(&str);
			}

			item.close();

			uint tmp = map.offset();
			map.seek(compressedSizeOffset);
			map.write(compressedSize);
			map.seek(tmp);
		}

		// Save the size of this section
		sectionSize = map.offset() - sizeOffset;

		map.seek(sizeOffset);
		map.write(sectionSize - 8);
		map.seek(sizeOffset + sectionSize);
	}

	map.write((uint)MAP_FOOTER);

	// Cleanup the compiled scripts folder
#ifdef _WIN32
	{
		SHFILEOPSTRUCT fileop = {0};
		fileop.wFunc = FO_DELETE;
		fileop.pFrom = "compile\0";
		fileop.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
		SHFileOperation(&fileop);
	}
#endif
}

void mapCompileAll(const char * dir, const char * prefix)
{
	intptr_t find;
	string mapname, builddir, search = dir;
	_finddata32_t data;

	search += "/*";

	find = _findfirst32(search.c_str(), &data);

	if(find == -1)
		return;

	do
	{
		if(_stricmp(data.name, ".") == 0 || _stricmp(data.name, "..") == 0)
			continue;

		if(data.attrib & _A_SUBDIR)
		{
			mapname = prefix;
			mapname += "_";
			mapname += data.name;
			mapname += ".nym";

			builddir = dir;
			builddir += "/";
			builddir += data.name;

			mapCompile(mapname.c_str(), builddir.c_str(), builddir.c_str());
		}
	} while(_findnext32(find, &data) == 0);
}

void mapCompileAll(const char * dir)
{
	mapCompileAll(dir, dir);
}

void mapCompilePatch(const char * dir, const char * prefix)
{
	intptr_t find;
	string mapname, builddir, prefixdir, search = dir;
	_finddata32_t data;

	search += "/*";

	find = _findfirst32(search.c_str(), &data);

	if(find == -1)
		return;

	do
	{
		if(_stricmp(data.name, ".") == 0 || _stricmp(data.name, "..") == 0)
			continue;

		if(data.attrib & _A_SUBDIR)
		{
			mapname = prefix;
			mapname += "_";
			mapname += data.name;
			mapname += "_patch.nym";

			builddir = dir;
			builddir += "/";
			builddir += data.name;

			prefixdir = prefix;
			prefixdir += "/";
			prefixdir += data.name;

			mapCompile(mapname.c_str(), builddir.c_str(), prefixdir.c_str());
		}
	} while(_findnext32(find, &data) == 0);
}
#endif // _DEBUG

void mapLoad(file& f, map_t& header)
{
	uint len;
	uint i, j, offset;
	ushort major, minor;
	string itemName;

	header.f = &f;
	header.deleteFile = false;

	// Check the magic
	if(f.readuint32() != MAP_MAGIC)
		dbgError("invalid map magic");

	// Check the version of the map
	major = f.readuint16();
	minor = f.readuint16();

	if(major < MAP_MAJOR || (major == MAP_MAJOR && minor < MAP_MINOR))
		dbgError("map is out of date");
	
	// Read in the flags
	header.flags = f.readuint32();

	// Read in the name
	itemName.load(f);
	if(itemName.length() + 1 > sizeof(header.name))
		dbgError("map name is too long");

	strcpy(header.name, itemName.c_str());

	// Read in the section information
	for(i = 0;i < MSectionCount;i++)
	{
		header.sections[i].size = f.readuint32();
		header.sections[i].itemCount = f.readuint32();
		if(header.sections[i].itemCount == 0)
			header.sections[i].items = NULL;
		else
			header.sections[i].items = (sectionitem_t*)malloc(sizeof(sectionitem_t) * header.sections[i].itemCount);

		// store the next section offset
		offset = f.offset() + header.sections[i].size;
		
		// Read in the item meta
		for(j = 0;j < header.sections[i].itemCount;j++)
		{
			// Item index
			header.sections[i].items[j].index = j;

			// Item size
			header.sections[i].items[j].size = f.readuint32();
			header.sections[i].items[j].compressedSize = f.readuint32();
			
			// Item name
			itemName.load(f);
			len = strlen(itemName.c_str()) + 1;
			header.sections[i].items[j].name = (char*)malloc(len);
			memcpy(header.sections[i].items[j].name, itemName.c_str(), len);
			header.sections[i].items[j].nameHash = itemName.getHash();

			// Item offset
			header.sections[i].items[j].dataOffset = f.offset();

			// Item data
			header.sections[i].items[j].data = NULL;

			// Seek to the next item
			if(header.sections[i].items[j].compressedSize)
				f.seek(f.offset() + header.sections[i].items[j].compressedSize);
			else
				f.seek(f.offset() + header.sections[i].items[j].size);
		}

		// Go to the next section
		f.seek(offset);
	}

	if(f.readuint32() != MAP_FOOTER)
		dbgError("map has invalid footer");
}

void mapLoad(const char * name, map_t& header)
{
	file * f = new file();

	if(!f->openRead(name))
		dbgError("unable to open map '%s'", name);

	mapLoad(*f, header);
	header.deleteFile = true;

	if(_stricmp(name, header.name) != 0)
		dbgError("map name mismatch");
}

bool mapTryLoad(const char * name, map_t& header)
{
	file * f = new file();

	if(!f->openRead(name))
	{
		delete f;
		return false;
	}

	mapLoad(*f, header);
	header.deleteFile = true;
	return true;
}

sectionitem_t * mapLoadItem(map_t& header, uint section, uint item)
{
	z_stream str;
	uint size, read;
	char compBuffer[1024 * 2];
	sectionitem_t& sectionitem = header.sections[section].items[item];

	if(item >= header.sections[section].itemCount)
		dbgError("invalid section item; cannot load");

	if(sectionitem.data != NULL)
		return &sectionitem;

	// Allocate a buffer to store the data
	sectionitem.data = (byte*)malloc(sectionitem.size);

	if(sectionitem.compressedSize)
	{
		// Decompress the data
		memset(&str, 0, sizeof(str));
		if(inflateInit(&str) < Z_OK)
			dbgError("inflateInit failed");

		str.next_out = sectionitem.data;
		str.avail_out = sectionitem.size;

		header.f->seek(sectionitem.dataOffset);

		size = sectionitem.compressedSize;
		while(size)
		{
			read = sizeof(compBuffer);
			if(read > size)
				read = size;

			size -= read;

			header.f->read(compBuffer, read);

			str.next_in = (Bytef*)compBuffer;
			str.avail_in = read;

			while(str.avail_in)
			{
				if(inflate(&str, Z_NO_FLUSH) < Z_OK)
					dbgError("inflate failed");
			}
		}

		inflateEnd(&str);
	}
	else
	{
		// Read in the data
		header.f->seek(sectionitem.dataOffset);
		header.f->read(sectionitem.data, sectionitem.size);
	}

	// Done!
	return &sectionitem;
}

void mapLoadSection(map_t& header, uint section)
{
	uint i;
	for(i = 0;i < header.sections[section].itemCount;i++)
		mapLoadItem(header, section, i);
}

void mapUnloadItem(sectionitem_t* item)
{
	if(item->data != NULL)
	{
		free(item->data);
		item->data = NULL;
	}
}

void mapUnloadItem(map_t& header, uint section, uint item)
{
	mapUnloadItem(&header.sections[section].items[item]);
}

void mapUnloadSection(map_t& header, uint section)
{
	uint i;
	for(i = 0;i < header.sections[section].itemCount;i++)
		mapUnloadItem(&header.sections[section].items[i]);
}

void mapUnload(map_t& header)
{
	int i;
	uint j;
	section_t * section;

	// First unload the sections
	for(i = 0;i < MSectionCount;i++)
	{
		mapUnloadSection(header, i);

		section = &header.sections[i];

		// The name is allocated, free it
		for(j = 0;j < section->itemCount;j++)
			free(section->items[j].name);

		// Now free the section items
		free(section->items);
	}
	
	// Map has been unloaded, now close the file handle
	header.f->close();

	if(header.deleteFile)
		delete header.f;
}

sectionitem_t * mapLookupItem(map_t& header, uint section, uint item, bool load)
{
	sectionitem_t& ptr = header.sections[section].items[item];;

	if(load && ptr.data == NULL)
	{
		// Attempt to load the item as requested
		mapLoadItem(header, section, item);
		return mapLookupItem(header, section, item, false);
	}
	
	return &ptr;
}

sectionitem_t * mapLookupItem(map_t& header, uint section, const char * name, bool load)
{
	uint i;
	string str(name);

	for(i = 0;i < header.sections[section].itemCount;i++)
	{
		if(str.getHash() == header.sections[section].items[i].nameHash && str == name)
		{
			// Load the item if requested
			if(load && header.sections[section].items[i].data == NULL)
				mapLoadItem(header, section, i);

			return &header.sections[section].items[i];
		}
	}

	// Item does not exist in the map
	return NULL;
}
