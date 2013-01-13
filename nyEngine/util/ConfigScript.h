/*
Name: ConfigScript.h
Author: John Judnich
Purpose: See ConfigScript.cpp

Source:
http://www.ogre3d.org/tikiwiki/tiki-index.php?page=All-purpose+script+parser
*/

#ifndef _CONFIGSCRIPT_H__
#define _CONFIGSCRIPT_H__
 
#include <OgreScriptLoader.h>
#include <OgreStringConverter.h>
#include <hash_map>
#include <vector>
 
class ConfigNode;
 
class ConfigScriptLoader
{
public:
    ConfigScriptLoader();
    ~ConfigScriptLoader();
 
    inline static ConfigScriptLoader &getSingleton() { return *singletonPtr; }
    inline static ConfigScriptLoader *getSingletonPtr() { return singletonPtr; }
 
    Ogre::Real getLoadingOrder() const;
    const Ogre::StringVector &getScriptPatterns() const;
 
    ConfigNode *getConfigScript(const Ogre::String &type, const Ogre::String &name);
 
    void parseScript(Ogre::DataStreamPtr &stream, map_t *parentMap);
	void purgeMap(map_t *map);

	stdext::hash_map<Ogre::String, ConfigNode*> scriptList;
 
private:
    static ConfigScriptLoader *singletonPtr;
 
    Ogre::Real mLoadOrder;
    Ogre::StringVector mScriptPatterns;
 
    //Parsing
    char *parseBuff, *parseBuffEnd, *buffPtr;
    size_t parseBuffLen;
	map_t *pMap;
 
    enum Token
    {
        TOKEN_Text,
        TOKEN_NewLine,
        TOKEN_OpenBrace,
        TOKEN_CloseBrace,
        TOKEN_EOF,
    };
 
    Token tok, lastTok;
    Ogre::String tokVal, lastTokVal;
    char *lastTokPos;
 
    void _parseNodes(ConfigNode *parent);
    void _nextToken();
    void _prevToken();
};
 
class ConfigNode
{
public:
    ConfigNode(ConfigNode *parent, map_t *map, const Ogre::String &name = "untitled");
    ~ConfigNode();
 
    inline void setName(const Ogre::String &name)
    {
        this->name = name;
    }
 
    inline Ogre::String &getName()
    {
        return name;
    }
 
    inline void addValue(const Ogre::String &value)
    {
        values.push_back(value);
    }
 
    inline void clearValues()
    {
        values.clear();
    }
 
    inline std::vector<Ogre::String> &getValues()
    {
        return values;
    }
 
    inline const Ogre::String &getValue(unsigned int index = 0)
    {
        assert(index < values.size());
        return values[index];
    }
 
    inline float getValueF(unsigned int index = 0)
    {
        assert(index < values.size());
        return Ogre::StringConverter::parseReal(values[index]);
    }
 
    inline double getValueD(unsigned int index = 0)
    {
        assert(index < values.size());
 
        std::istringstream str(values[index]);
        double ret = 0;
        str >> ret;
        return ret;
    }
 
    inline int getValueI(unsigned int index = 0)
    {
        assert(index < values.size());
        return Ogre::StringConverter::parseInt(values[index]);
    }
 
    ConfigNode *addChild(const Ogre::String &name = "untitled", bool replaceExisting = false);
    ConfigNode *findChild(const Ogre::String &name, bool recursive = false);
 
    inline std::vector<ConfigNode*> &getChildren()
    {
        return children;
    }
 
    inline ConfigNode *getChild(unsigned int index = 0)
    {
        assert(index < children.size());
        return children[index];
    }
 
    void setParent(ConfigNode *newParent);
 
    inline ConfigNode *getParent()
    {
        return parent;
    }

	map_t *parentMap;
 
private:
    Ogre::String name;
    std::vector<Ogre::String> values;
    std::vector<ConfigNode*> children;
    ConfigNode *parent;
 
    int lastChildFound;  //The last child node's index found with a call to findChild()
 
    std::vector<ConfigNode*>::iterator _iter;
    bool _removeSelf;
};
 
#endif