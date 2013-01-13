/*
Name: CMapLoader.h
Author: Nathan LeRoux
Purpose: See CMapLoader.cpp
*/

#ifndef _CMAPLOADER_H
#define _CMAPLOADER_H

typedef struct _LOADED_MAP
{
	map_t * map; // The map file, always filled out
	map_t * patch; // The patch file, may be NULL if not present
	bool stayLoaded; // If this map should remain when unloading levels
} LOADED_MAP;

template<class T> struct MapResource
{
	LOADED_MAP map; // The map this resource belongs to
	T data; // The data this resource points to
};

// Provides various functions for loading resources from maps
class CMapLoader
{
	friend class CMapArchive;

public:
	CMapLoader();
	~CMapLoader();

	void init(class CLuaManager * lua);
	void deinit();
	// Load the first item with this name
	sectionitem_t* LoadItem(const char * path, uint section, LOADED_MAP * map = NULL);
	sectionitem_t* LoadItem(const char * path, LOADED_MAP * map = NULL);
	// Find the item, but do not load it
	sectionitem_t* FindItem(const char * path, LOADED_MAP * map = NULL);
	// Load and unload stuff
	void SetupScripts();
	void LoadMap(const char * name, LOADED_MAP * map = NULL, bool keepLoaded = false);
	void UnloadMap(LOADED_MAP& map);
	// Unloading a map will also unload it's patch
	void UnloadMap(map_t * map);
	Ogre::MeshPtr LoadModel(const char * path);
	void UnloadModel(Ogre::MeshPtr mesh);
	void LoadMaterials(map_t * map);
	void LoadScripts(map_t * map);
	void UnloadModels(map_t * map);
	void ReloadMaterials();
	void UnloadScripts(map_t * map);

private:
	void mapAdd(LOADED_MAP& map);

	bool hasInit;
	class CLuaManager * luaManager;
	std::vector<LOADED_MAP> mapList;
	std::vector<MapResource<Ogre::MeshPtr>> meshes;
};

#endif