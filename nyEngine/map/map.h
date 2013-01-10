#ifndef _MAP_H
#define _MAP_H

// The minimum supported map build
#define MAP_MAJOR 1
#define MAP_MINOR 0

#define MAP_MAGIC 'PMYN' // 'NYMP' little endian
#define MAP_FOOTER 'TFYN' // 'NYFT' little endian

enum
{
	MSectionZone,
	MSectionModel,
	MSectionTexture,
	MSectionMaterial,
	MSectionShader,
	MSectionSound,
	MSectionEntity,
	MSectionScript,
	MSectionObjectScript,
	MSectionGeneric,
	MSectionCount
};

typedef struct sectionitem_s
{
	uint index; // which item this is
	uint size; // the size of the data
	uint compressedSize; // the size of the compressed data
	char * name; // the name of the item
	uint nameHash; // the hashtag of the name
	uint dataOffset; // the offset of the data
	byte * data; // the data buffer
} sectionitem_t;

typedef struct section_s
{
	uint size; // the size of all the section items
	uint itemCount; // how many items are in this section

	// Item list
	sectionitem_t * items;
} section_t;

typedef struct map_s
{
	// the file handle used to load map resources
	file * f;

	// if we should delete the file handle on map deletion
	bool deleteFile;

	// map flags
	uint flags;

	// map name
	char name[0x40];

	// the sections
	section_t sections[MSectionCount];
} map_t;

// Compile a map
void mapCompile(const char * filename, const char * path, const char * prefix);
void mapCompileAll(const char * dir);
void mapCompilePatch(const char * dir, const char * prefix);

// Load the header of a map
void mapLoad(file& f, map_t& header);
void mapLoad(const char * name, map_t& header);
bool mapTryLoad(const char * name, map_t& header);

// Load a single item from a section
sectionitem_t * mapLoadItem(map_t& header, uint section, uint item);

// Load an entire section of a map
void mapLoadSection(map_t& header, uint section);

// Unload a section item
void mapUnloadItem(sectionitem_t* item);
void mapUnloadItem(map_t& header, uint section, uint item);

// Unload an entire section
void mapUnloadSection(map_t& header, uint section);

// Unload an entire map
void mapUnload(map_t& header);

// Lookup a section item
sectionitem_t * mapLookupItem(map_t& header, uint section, uint item, bool load = true);
sectionitem_t * mapLookupItem(map_t& header, uint section, const char * name, bool load = true);

// Map format notes
/*
// Header
uint MAP_MAGIC
ushort version major
ushort version minor
uint flags

string name

// Map sections are here
// Section format:
// uint section size (size of section items)
// uint item count
// section items : item count
// {
//     uint item length
//     uint compressed length // if zero, item is not compressed
//     string name
//     // other data
// }

// Zone (geometry)
{
	// zone data here (geometry, etc)
}

// Models
{
	// model data
}

// Textures
{
	// texture data
}

// Entity (objects)
{
	// entity data
}

// Scripts
{
	// because of how lua interperets scripts, the data can be compiled or raw, it doesn't matter
	
	string name
	char[] data // length is equal to the item length - name length
}

// Sounds
{
	// audio data
}

*/

#endif