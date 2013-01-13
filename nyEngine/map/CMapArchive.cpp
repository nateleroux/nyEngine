/*
Name: CMapArchive.cpp
Author: Nathan LeRoux
Purpose: Allows OGRE to use my map files when searching for resources
*/

#define SUPPRESS_DEBUG_NEW

#include "..\include.h"
#include <OgreArchiveManager.h>
#include <OgreArchiveFactory.h>
#include <OgreArchive.h>
#include <OgreResourceManager.h>
#include "CMapLoader.h"
#include "CMapArchive.h"

CMapArchive::CMapArchive(const Ogre::String& name, const Ogre::String& archType)
	: Archive(name, archType)
{
	mReadOnly = true;
}

Ogre::DataStreamPtr CMapArchive::open(const Ogre::String& filename, bool readOnly) const
{
	dbgOut("opening file '%s' from map archive", filename.c_str());

	if(!readOnly)
		dbgError("attempted to open a map for write access");

	sectionitem_t * item = loader->LoadItem(filename.c_str());

	if(item == NULL)
		dbgError("attempted to open an item that does not exist");

	Ogre::DataStreamPtr ptr(new Ogre::MemoryDataStream(item->size));
	
	ptr->write(item->data, item->size);
	ptr->seek(0);

	mapUnloadItem(item);

	return ptr;
}

Ogre::StringVectorPtr CMapArchive::list(bool recursive, bool dirs)
{
	dbgError("not implemented");
	return Ogre::StringVectorPtr();
}

Ogre::FileInfoListPtr CMapArchive::listFileInfo(bool recursive, bool dirs)
{
	dbgError("not implemented");
	return Ogre::FileInfoListPtr();
}

Ogre::StringVectorPtr CMapArchive::find(const Ogre::String& pattern, bool recursive, bool dirs)
{
	return Ogre::StringVectorPtr(new Ogre::StringVector());
}

bool CMapArchive::exists(const Ogre::String& filename)
{
	if(loader->FindItem(filename.c_str(), NULL))
		return true;
	else
	{
		dbgOut("file '%s' not found in any loaded maps", filename.c_str());
		return false;
	}
}

time_t CMapArchive::getModifiedTime(const Ogre::String& filename)
{
	return 0;
}

Ogre::FileInfoListPtr CMapArchive::findFileInfo(const Ogre::String& pattern, bool recursive, bool dirs) const
{
	dbgError("not implemented");
	return Ogre::FileInfoListPtr();
}

CMapArchiveFactory::CMapArchiveFactory()
{
	hasInit = false;
}

CMapArchiveFactory::~CMapArchiveFactory()
{
	Shutdown();
}

void CMapArchiveFactory::Init(CMapLoader * Loader)
{
	if(hasInit)
		return;

	loader = Loader;
	Ogre::ArchiveManager::getSingleton().addArchiveFactory(this);

	hasInit = true;
}

void CMapArchiveFactory::Shutdown()
{
	if(!hasInit)
		return;

	hasInit = false;
}

const Ogre::String& CMapArchiveFactory::getType() const
{
	static Ogre::String type = "nyEngine Map Archive";
	return type;
}

Ogre::Archive * CMapArchiveFactory::createInstance(const Ogre::String& name)
{
	CMapArchive * arch = OGRE_NEW CMapArchive(name, getType());
	arch->loader = loader;

	return arch;
}

void CMapArchiveFactory::destroyInstance(Ogre::Archive * archive)
{
	delete (CMapArchive*)archive;
}