/*
Name: LuaManager.h
Author: Nathan LeRoux
Purpose: See LuaManager.cpp
*/

#ifndef _LUAMANAGER_H
#define _LUAMANAGER_H

#include "..\lua\lua.hpp"
#include <vector>

// A special entity value, this represents the level object
#define ENTITY_LEVEL ((void*)(-1))

#define ONNOTIFY_EVENT_COUNT 16
#define ENDON_EVENT_COUNT 16
#define ENTITY_EVENT_NAME_LENGTH 32

typedef struct _ENTITY_EVENT
{
	char event[ENTITY_EVENT_NAME_LENGTH];
	void * entity;
} ENTITY_EVENT;

typedef struct _EVENT_NOTIFY
{
	ENTITY_EVENT evt;
	int funcReference;
} EVENT_NOTIFY;

// Lua thread struct
typedef struct _LUA_THREAD
{
	// The lua thread
	lua_State *L;
	// The argument count upon entry to the thread
	int nargs;
	// If this is the first run of the thread
	bool firstRun;
	// If this thread should be terminated before it has a chance to run again
	bool terminate;
	// The unique ID of this thread
	void * id;
	// How long the thread is sleeping for in seconds
	double waittime;
	// The event we are waiting for
	ENTITY_EVENT waitEvent;
	// The endon event list
	ENTITY_EVENT endonEvents[ENDON_EVENT_COUNT];
} LUA_THREAD;

class CLuaManager
{
public:
	CLuaManager();
	~CLuaManager();

	// Compile a script
	static void Compile(file& in, file& out, const char * filename);

	// Initialize
	void Init();

	// Perform a single script tick
	void Tick(double delta, const char * Event = NULL);

	// Load script(s)
	void LoadScript(sectionitem_t * item);
	void LoadScripts(map_t * map, map_t * patch);

	// Create a thread
	lua_State * CreateThread(int nargs);
	int FindThread(lua_State * L, std::vector<LUA_THREAD> ** v);

	// Notify threads of an event
	// The argument should be either ENTITY_LEVEL or an entity ID
	void Notify(void * Entity, const char * Event, void * Argument = NULL);

	// Reset the script state
	void ResetState();

	// Save/Load the script state from a file
	void save(file& f);
	void load(file& f);

	static CLuaManager * singleton;

	std::vector<LUA_THREAD> queueThreads, tempQueue;
	std::vector<LUA_THREAD> luaThreads;
	EVENT_NOTIFY onnotify[ONNOTIFY_EVENT_COUNT];

	void addLuaFunction(lua_CFunction func, const char * name);

private:
	std::vector<void *> functionBindings;
	std::vector<std::string> functionNames;
	std::vector<LUA_THREAD>::iterator tickThread(std::vector<LUA_THREAD>::iterator i, std::vector<LUA_THREAD> * v, double delta, const char * Event);
	void setupLuaFunctions();
	void * getNewThreadId();

	bool didInit;

	lua_State *L;
};

#endif