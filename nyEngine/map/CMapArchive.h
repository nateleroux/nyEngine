/*
Name: CMapArchive.h
Author: Nathan LeRoux
Purpose: See CMapArchive.cpp
*/

#ifndef _CMAPARCHIVE_H
#define _CMAPARCHIVE_H

// This class represents an abstract view into any loaded maps
class CMapArchive : Ogre::Archive
{
	friend class CMapArchiveFactory;

public:
	CMapArchive(const Ogre::String& name, const Ogre::String& archType);

	bool isCaseSensitive() const { return false; }
	void load() { }
	void unload() { }
	Ogre::DataStreamPtr open(const Ogre::String& filename, bool readOnly = true) const;
	Ogre::StringVectorPtr list(bool recursive = true, bool dirs = false);
	Ogre::FileInfoListPtr listFileInfo(bool recursive = true, bool dirs = false);
	Ogre::StringVectorPtr find(const Ogre::String& pattern, bool recursive = true, bool dirs = false);
	bool exists(const Ogre::String& filename);
	time_t getModifiedTime(const Ogre::String& filename);
	Ogre::FileInfoListPtr findFileInfo(const Ogre::String& pattern, bool recursive = true, bool dirs = false) const;

private:
	CMapLoader * loader;
};

class CMapArchiveFactory : Ogre::ArchiveFactory
{
public:
	CMapArchiveFactory();
	~CMapArchiveFactory();

	void Init(CMapLoader * Loader);
	void Shutdown();

	const Ogre::String& getType() const;
	Ogre::Archive * createInstance(const Ogre::String& name);
	void destroyInstance(Ogre::Archive * archive);

private:
	bool hasInit;
	CMapLoader * loader;
};

#endif