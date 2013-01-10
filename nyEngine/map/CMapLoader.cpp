#include "..\include.h"

#include <OgreRoot.h>
#include <OgreMeshSerializer.h>
#include <OgreMeshManager.h>

#include "..\util\LuaManager.h"
#include "CMapLoader.h"
#include "..\util\ConfigScript.h"

CMapLoader::CMapLoader()
{
	hasInit = false;
}

CMapLoader::~CMapLoader()
{
	deinit();
}

void CMapLoader::init(CLuaManager * lua)
{
	if(hasInit)
		return;

	luaManager = lua;
	hasInit = true;
}

void CMapLoader::deinit()
{
	if(!hasInit)
		return;

	hasInit = false;

	while(mapList.size())
	{
		UnloadMap(mapList[0]);
	}
}

void CMapLoader::mapAdd(LOADED_MAP& map)
{
	mapList.push_back(map);

	if(map.patch)
	{
		LoadMaterials(map.patch);
		LoadScripts(map.patch);
	}

	LoadMaterials(map.map);
	LoadScripts(map.map);
}

sectionitem_t* CMapLoader::LoadItem(const char * path, uint section, LOADED_MAP * map)
{
	uint i;
	sectionitem_t* item;
	for(i = 0;i < mapList.size();i++)
	{
		if(((mapList[i].patch) && ((item = mapLookupItem(*mapList[i].patch, section, path)))) ||
				(item = mapLookupItem(*mapList[i].map, section, path)))
		{
			*map = mapList[i];
			return item;
		}
	}

	return NULL;
}

sectionitem_t* CMapLoader::LoadItem(const char * path, LOADED_MAP * map)
{
	uint i, j;
	sectionitem_t* item;
	for(i = 0;i < mapList.size();i++)
	{
		for(j = 0;j < MSectionCount;j++) 
		{
			if(((mapList[i].patch) && ((item = mapLookupItem(*mapList[i].patch, j, path)))) ||
				(item = mapLookupItem(*mapList[i].map, j, path)))
			{
				if(map)
					*map = mapList[i];

				return item;
			}
		}
	}

	return NULL;
}

sectionitem_t* CMapLoader::FindItem(const char * path, LOADED_MAP * map)
{
	uint i, j;
	sectionitem_t* item;
	for(i = 0;i < mapList.size();i++)
	{
		for(j = 0;j < MSectionCount;j++) 
		{
			if(((mapList[i].patch) && ((item = mapLookupItem(*mapList[i].patch, j, path, false)))) ||
				(item = mapLookupItem(*mapList[i].map, j, path, false)))
			{
				if(map)
					*map = mapList[i];

				return item;
			}
		}
	}

	return NULL;
}

void CMapLoader::SetupScripts()
{
	luaManager->ResetState();

	std::vector<LOADED_MAP>::iterator i;
	for(i = mapList.begin();i != mapList.end();i++)
		luaManager->LoadScripts(i->map, i->patch);
}

void CMapLoader::LoadMap(const char * name, LOADED_MAP * map, bool keepLoaded)
{
	LOADED_MAP ldmap;
	map_t * loadedMap, * loadedPatch;
	string mapPath, patchPath;

	dbgOut("loading map '%s'", name);

	// map = name.nym
	// patch = name_patch.nym
	loadedMap = new map_t();
	loadedPatch = new map_t();

	mapPath = name;
	mapPath += ".nym";
	patchPath = name;
	patchPath += "_patch.nym";

	mapLoad(mapPath.c_str(), *loadedMap);
	if(!mapTryLoad(patchPath.c_str(), *loadedPatch))
	{
		delete loadedPatch;
		loadedPatch = NULL;
	}
	else
		dbgOut("map '%s' has patch", name);

	// Add the patch before the map so that the patch resources are used first
	ldmap.map = loadedMap;
	ldmap.patch = loadedPatch;
	ldmap.stayLoaded = keepLoaded;

	mapAdd(ldmap);

	if(map)
	{
		memcpy(map, &ldmap, sizeof(ldmap));
	}
}

void CMapLoader::UnloadMap(LOADED_MAP& map)
{
	// The patch is automatically unloaded
	UnloadMap(map.map);
}

void CMapLoader::UnloadMap(map_t * map)
{
	dbgOut("unloading map '%s'", map->name);

	// Delete all resources loaded from this map
	UnloadModels(map);
	UnloadScripts(map);

	std::vector<LOADED_MAP>::iterator i;
	for(i = mapList.begin();i != mapList.end();)
	{
		if(i->map == map || i->patch == map)
		{
			mapUnload(*i->map);
			if(i->patch)
				mapUnload(*i->patch);

			delete i->map;
			delete i->patch;

			i = mapList.erase(i);
		}
		else
			i++;
	}

	// Reload our material scripts
	ReloadMaterials();
}

Ogre::MeshPtr CMapLoader::LoadModel(const char * path)
{
	MapResource<Ogre::MeshPtr> res;
	Ogre::MeshPtr pMesh;

	// Check if this model was already loaded
	pMesh = Ogre::MeshManager::getSingleton().getByName(path);
	if(!pMesh.isNull())
		return pMesh;

	sectionitem_t * item = LoadItem(path, &res.map);
	if(item == NULL)
		return Ogre::MeshPtr(NULL);

	pMesh = Ogre::MeshManager::getSingleton().createManual(path,
		Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);

	Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream(item->data, item->size, false, true));
	Ogre::MeshSerializer serializer;
	serializer.importMesh(stream, pMesh.getPointer());

	// No need to keep this in memory
	mapUnloadItem(item);

	// Track this resource
	res.data = pMesh;
	meshes.push_back(res);
	
	return pMesh;
}

void CMapLoader::UnloadModel(Ogre::MeshPtr mesh)
{
	Ogre::ResourceHandle handle = mesh->getHandle();
	// Ogre::MeshManager::getSingleton().unload(handle);
	Ogre::MeshManager::getSingleton().remove(handle);

	std::vector<MapResource<Ogre::MeshPtr>>::iterator i;
	for(i = meshes.begin();i != meshes.end();i++)
	{
		if(i->data.getPointer() == mesh.getPointer())
		{
			meshes.erase(i);
		}
	}
}

void CMapLoader::LoadMaterials(map_t * map)
{
	// TODO: consider unloading material scripts somehow...
	for(uint i = 0;i < map->sections[MSectionMaterial].itemCount;i++)
	{
		sectionitem_t* item = mapLookupItem(*map, MSectionMaterial, i);

		// Parse the material code
		Ogre::DataStreamPtr sourcePtr(new Ogre::MemoryDataStream(item->data, item->size, false, true));
		Ogre::MaterialManager::getSingleton().parseScript(sourcePtr,
			Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		
		// Unload the item, it is no longer needed
		mapUnloadItem(item);
	}
}

void CMapLoader::LoadScripts(map_t * map)
{
	for(uint i = 0;i < map->sections[MSectionObjectScript].itemCount;i++)
	{
		sectionitem_t* item = mapLookupItem(*map, MSectionObjectScript, i);

		// Parse the script
		Ogre::DataStreamPtr ptr(new Ogre::MemoryDataStream(item->data, item->size, false, true));
		ConfigScriptLoader::getSingleton().parseScript(ptr, map);
		ptr.setNull();

		// Unload the item
		mapUnloadItem(item);
	}
}

void CMapLoader::UnloadModels(map_t * map)
{
	std::vector<MapResource<Ogre::MeshPtr>>::iterator i;
	for(i = meshes.begin();i != meshes.end();)
	{
		if(i->map.map == map || i->map.patch == map)
		{
			Ogre::MeshManager::getSingleton().remove(i->data->getName());
			i = meshes.erase(i);
		}
		else
			i++;
	}
}

void CMapLoader::ReloadMaterials()
{
	Ogre::MaterialManager::getSingleton().removeAll();
	
	std::vector<LOADED_MAP>::iterator i;
	for(i = mapList.begin();i != mapList.end();i++)
	{
		LoadMaterials(i->map);
		LoadMaterials(i->patch);
	}
}

void CMapLoader::UnloadScripts(map_t * map)
{
	ConfigScriptLoader::getSingleton().purgeMap(map);
}
