#include "..\include.h"
#include "LuaManager.h"
#include "..\game\game.h"
#include "..\Gorilla\GorillaLua.h"
extern "C" {
#include "..\pluto\pluto.h"
}

#define LUA_ERROR(L) dbgError("script error: %s", lua_tostring(L, -1))

// The lua program that prepares namespaces and loads functions into them
const char luaNamespacePrepFunction[] =
"-- utility function to throw an object onto the anti-persist stack\n" \
"-- because of how the scripts work, this will always be called\n" \
"-- in the same order on the same version of the game\n" \
"function antipersist (obj)\n" \
"    AntiPersist[obj] = AntiPersistCount" \
"    PersistRestore[AntiPersistCount] = obj\n" \
"    AntiPersistCount = AntiPersistCount + 1\n" \
"end\n" \
"-- utility function to split a string\n" \
"function splitString (str, sepChars)\n" \
"    local tab = { }\n" \
"    local tabIndex = 1\n" \
"    local strIndex = 1\n" \
"    local len = strlen(str)\n" \
"    local len2 = strlen(sepChars)\n" \
"    \n" \
"    for index = 1, len do\n" \
"        for index2 = 1, len2 do\n" \
"            if strchar(str, index) == strchar(sepChars, index2) then\n" \
"                tab[tabIndex] = strsub(str, strIndex, index - strIndex)\n" \
"                tabIndex = tabIndex + 1\n" \
"                strIndex = index + 1\n" \
"                break\n" \
"            end\n" \
"        end\n" \
"    end\n" \
"    \n" \
"    if strIndex < len then\n" \
"        tab[tabIndex] = strsub(str, strIndex, len - strIndex + 1)\n" \
"    end\n" \
"    \n" \
"    return tab\n" \
"end\n" \
"-- run 'func' under the context of table 'namespace'\n" \
"function loadFuncIntoNamespace (namespace, func)\n" \
"    -- Split the name\n" \
"    local names = splitString(namespace, \"/\")\n" \
"\n" \
"    -- Create the namespaces\n" \
"    local table = _G\n" \
"\n" \
"    print(\"walking table\")\n" \
"    for index, value in ipairs(names) do\n" \
"        print(\"creating table: \" .. value)\n" \
"        if table[value] == nil then\n" \
"            table[value] = { }\n" \
"        end\n" \
"        table = table[value]\n" \
"    end\n" \
"\n" \
"    -- set the metatable of the file namespace\n" \
"    setmetatable(table, { __index = _G })\n" \
"\n" \
"    -- change the file's environment to the namespace\n" \
"    setfenv(func, table)\n" \
"    func()\n" \
"    -- now create a thread for the init() function, there are no arguments for init\n" \
"    -- the init function has no owner\n" \
"    if(table.init ~= nil) then\n" \
"        createThread(nil, table.init)\n" \
"    return table\n" \
"    end\n" \
"end\n" \
"function deepcopy(orig)\n" \
"    local orig_type = type(orig)\n" \
"    local copy\n" \
"    if orig_type == 'table' then\n" \
"        copy = {}\n" \
"        for orig_key, orig_value in next, orig, nil do\n" \
"            copy[deepcopy(orig_key)] = deepcopy(orig_value)\n" \
"        end\n" \
"        setmetatable(copy, deepcopy(getmetatable(orig)))\n" \
"    else -- number, string, boolean, etc\n" \
"        copy = orig\n" \
"    end\n" \
"    return copy\n" \
"end\n" \
"-- ensure these functions aren't persisted\n" \
"antipersist(deepcopy)\n" \
"antipersist(loadFuncIntoNamespace)\n" \
"antipersist(splitString)\n" \
"antipersist(antipersist)\n" \
"-- ensure that the tables aren't persisted either\n" \
"antipersist(AntiPersist)\n" \
"antipersist(PersistRestore)\n";

CLuaManager * CLuaManager::singleton;

int luaFileWriter(lua_State * L, const void * p, size_t sz, void * ud)
{
	file& out = *(file*)ud;

	out.write(p, sz);

	return 0;
}

void CLuaManager::Compile(file& in, file& out, const char * filename)
{
	char * buf = (char*)malloc(in.size());
	lua_State * L = luaL_newstate();
	
	if(buf == NULL)
		dbgError("CLuaManager::Compile - out of memory");

	in.read(buf, in.size());
	
	if(luaL_loadbuffer(L, buf, in.size(), filename) != 0)
		LUA_ERROR(L);
	
	lua_dump(L, luaFileWriter, &out);
	lua_close(L);

	free(buf);
}

CLuaManager::CLuaManager()
{
	singleton = this;
	didInit = false;
}

CLuaManager::~CLuaManager()
{
	if(didInit)
	{
		luaThreads.clear();
		queueThreads.clear();
		tempQueue.clear();

		if(L)
			lua_close(L);
	}
}

void CLuaManager::Init()
{
	if(didInit)
		return;

	didInit = true;

	L = NULL;
	ResetState();
}

void CLuaManager::Tick(double delta, const char * Event)
{
	std::vector<LUA_THREAD>::iterator i;

	// Tick each thread
	for(i = luaThreads.begin();i != luaThreads.end();)
		i = tickThread(i, &luaThreads, delta, Event);

	// Run any queued thread once, allowing for threads to start waiting immediately
	while(queueThreads.size())
	{
		// Clone the thread queue
		for(i = queueThreads.begin();i != queueThreads.end();i++)
			tempQueue.push_back(*i);
		queueThreads.clear();

		// Tick the threads
		for(i = tempQueue.begin();i != tempQueue.end();)
			i = tickThread(i, &tempQueue, delta, Event);

		// Merge the queue into the main list
		for(i = tempQueue.begin();i != tempQueue.end();i++)
			luaThreads.push_back(*i);
		tempQueue.clear();
	}
}

void CLuaManager::LoadScript(sectionitem_t * item)
{
	char nspace[0x200];
	char * c;

	// Load the script
	if(luaL_loadbuffer(L, (const char *)item->data, item->size, item->name) != 0)
		LUA_ERROR(L);

	// Now process the script into the appropriate namespace
	c = strrchr(item->name, '.');
	memset(nspace, 0, sizeof(nspace));
	strncpy(nspace, item->name, c - item->name);
	
	lua_getglobal(L, "loadFuncIntoNamespace"); // the function we are calling
	lua_pushstring(L, nspace); // the namespace
	lua_pushvalue(L, -3); // the function that we just loaded
	if(lua_pcall(L, 2, 1, 0) != 0)
		LUA_ERROR(L);

	// loadFuncIntoNamespace returns the table the functions were loaded into
	// Walk that table, adding all functions to the anti-persist table
	lua_pushnil(L);
	while(lua_next(L, -2) != 0)
	{
		// -2 = key, -1 = value
		// Key is not important, value must be persisted
		lua_getglobal(L, "antipersist");
		lua_pushvalue(L, -2);
		if(lua_pcall(L, 1, 0, 0) != 0)
			LUA_ERROR(L);

		// remove value
		lua_pop(L, 1);
	}

	// All done!
}

void CLuaManager::LoadScripts(map_t * map, map_t * patch)
{
	if(patch)
	{
		for(uint i = 0;i < patch->sections[MSectionScript].itemCount;i++)
		{
			sectionitem_t * item = mapLoadItem(*patch, MSectionScript, i);
			LoadScript(item);
			mapUnloadItem(item);
		}
	}

	for(uint i = 0;i < map->sections[MSectionScript].itemCount;i++)
	{
		// If this script is present in the patch, ignore and move on
		if(patch && mapLookupItem(*patch, MSectionScript, map->sections[MSectionScript].items[i].name, false))
			continue;

		sectionitem_t * item = mapLoadItem(*map, MSectionScript, i);
		LoadScript(item);
		mapUnloadItem(item);
	}
}

lua_State * CLuaManager::CreateThread(int nargs)
{
	LUA_THREAD thr = {0};
	thr.L = lua_newthread(L);
	thr.firstRun = true;
	thr.nargs = nargs;
	thr.id = getNewThreadId();

	// Make sure that the thread can't get garbage collected
	// _G.ThreadList[L] = L
	lua_getglobal(thr.L, "ThreadList");
	lua_pushthread(thr.L);
	lua_pushthread(thr.L);
	lua_settable(thr.L, -3);

	// Remember the ID
	// _G.ThreadIDs[L] = thr.id
	lua_getglobal(thr.L, "ThreadIDs");
	lua_pushthread(thr.L);
	lua_pushlightuserdata(thr.L, thr.id);
	lua_settable(thr.L, -3);

	queueThreads.push_back(thr);
	return thr.L;
}

int CLuaManager::FindThread(lua_State * L, std::vector<LUA_THREAD> ** v)
{
	for(uint i = 0;i < luaThreads.size();i++)
	{
		if(luaThreads[i].L == L)
		{
			*v = &luaThreads;
			return i;
		}
	}

	for(uint i = 0;i < tempQueue.size();i++)
	{
		if(tempQueue[i].L == L)
		{
			*v = &tempQueue;
			return i;
		}
	}

	*v = NULL;
	return -1;
}

void CLuaManager::Notify(void * Entity, const char * Event, void * Argument)
{
	int j;
	std::vector<LUA_THREAD>::iterator i;
	for(i = luaThreads.begin();i != luaThreads.end();i++)
	{
		if(i->waitEvent.entity == Entity && _stricmp(i->waitEvent.event, Event) == 0)
		{
			// We have a hit, pull the thread out of the waiting state
			i->waitEvent.entity = NULL;
			memset(i->waitEvent.event, 0, ENTITY_EVENT_NAME_LENGTH);
		}

		for(j = 0;j < ENDON_EVENT_COUNT;j++)
		{
			if(i->endonEvents[j].entity)
			{
				if(i->endonEvents[j].entity == Entity && _stricmp(i->endonEvents[j].event, Event) == 0)
				{
					i->terminate = true;
					break;
				}
			}
			else
				break;
		}
	}

	for(i = queueThreads.begin();i != queueThreads.end();i++)
	{
		if(i->waitEvent.entity == Entity && _stricmp(i->waitEvent.event, Event) == 0)
		{
			// We have a hit, pull the thread out of the waiting state
			i->waitEvent.entity = NULL;
			memset(i->waitEvent.event, 0, ENTITY_EVENT_NAME_LENGTH);
		}

		for(j = 0;j < ENDON_EVENT_COUNT;j++)
		{
			if(i->endonEvents[j].entity)
			{
				if(i->endonEvents[j].entity == Entity && _stricmp(i->endonEvents[j].event, Event) == 0)
				{
					i->terminate = true;
					break;
				}
			}
			else
				break;
		}
	}

	// Now we want to run callbacks
	for(j = 0;j < ONNOTIFY_EVENT_COUNT;j++)
	{
		if(onnotify[j].evt.entity)
		{
			if(onnotify[j].evt.entity == Entity && _stricmp(onnotify[j].evt.event, Event) == 0)
			{
				// callback(argument)
				lua_State * L = CreateThread(1);
				lua_rawgeti(L, LUA_REGISTRYINDEX, onnotify[j].funcReference);
				lua_pushlightuserdata(L, Argument);
			}
		}
		else
			break;
	}
}

void CLuaManager::ResetState()
{
	luaThreads.clear();
	queueThreads.clear();
	tempQueue.clear();
	functionBindings.clear();
	functionNames.clear();
	
	if(L)
		lua_close(L);

	memset(onnotify, 0, sizeof(onnotify));

	// Initialize the main lua state
	L = luaL_newstate();
	
	// Setup some globals

	// Globals table
	lua_pushvalue(L, LUA_GLOBALSINDEX);
	lua_setglobal(L, "_G");

	// Thread list table
	lua_newtable(L);
	lua_setglobal(L, "ThreadList");

	// Thread id table
	lua_newtable(L);
	lua_setglobal(L, "ThreadIDs");

	// Function persist vars
	lua_newtable(L);
	lua_setglobal(L, "AntiPersist");
	lua_newtable(L);
	lua_setglobal(L, "PersistRestore");
	lua_pushnumber(L, 0);
	lua_setglobal(L, "AntiPersistCount");

	// The 'level' object is a special value that means the level 'entity'
	// Should we add this to the anti persistence table?
	// It is a static value, and fucking with it might screw with the scripts
	lua_pushlightuserdata(L, ENTITY_LEVEL);
	lua_setglobal(L, "level");

	// Import the namespace program
	if(luaL_loadbuffer(L, luaNamespacePrepFunction, strlen(luaNamespacePrepFunction), "internal") != 0)
		LUA_ERROR(L);
	if(lua_pcall(L, 0, LUA_MULTRET, 0) != 0)
		LUA_ERROR(L);

	// Setup the c function bindings
	setupLuaFunctions();
}

void CLuaManager::save(file& f)
{
	// Save the entire script state to disk
	// Consider having some sort of field on the thread objects so we know what thread is what

	// Flush the lua state to the file
	// We do not persist any of the C function bindings
	// For security reasons, we do not persist any functions either
	// This is mostly made possible by not allowing functions to be in any scope except the file
	lua_settop(L, 0);

	lua_getglobal(L, "AntiPersist");
	lua_pushvalue(L, LUA_GLOBALSINDEX);

	pluto_persist(L, luaFileWriter, &f); // pluto.persist(t, _G)
}

void CLuaManager::load(file& f)
{

}

void CLuaManager::addLuaFunction(lua_CFunction func, const char * name)
{
	functionBindings.push_back(func);
	functionNames.push_back(name);

	lua_pushcfunction(L, func);

	// name = func
	lua_setglobal(L, name);

	// fix up the anti persist table
	// antipersist(_G[name])
	lua_getglobal(L, "antipersist");
	lua_getglobal(L, name);
	if(lua_pcall(L, 1, 0, 0) != 0)
		LUA_ERROR(L);
}

// upvalue stuff
#pragma region crap
static void settabss (lua_State *L, const char *i, const char *v) {
  lua_pushstring(L, v);
  lua_setfield(L, -2, i);
}

static void settabsi (lua_State *L, const char *i, int v) {
  lua_pushinteger(L, v);
  lua_setfield(L, -2, i);
}

static void settabsb (lua_State *L, const char *i, int v) {
  lua_pushboolean(L, v);
  lua_setfield(L, -2, i);
}

static lua_State *getthread (lua_State *L, int *arg) {
  if (lua_isthread(L, 1)) {
    *arg = 1;
    return lua_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;
  }
}

static void treatstackoption (lua_State *L, lua_State *L1, const char *fname) {
  if (L == L1) {
    lua_pushvalue(L, -2);
    lua_remove(L, -3);
  }
  else
    lua_xmove(L1, L, 1);
  lua_setfield(L, -2, fname);
}
#pragma endregion

static void getfunc (lua_State *L, int opt)
{
	if (lua_isfunction(L, 1))
		lua_pushvalue(L, 1);
	else
	{
		lua_Debug ar;
		int level = opt ? luaL_optint(L, 1, 1) : luaL_checkint(L, 1);
		luaL_argcheck(L, level >= 0, 1, "level must be non-negative");
		if (lua_getstack(L, level, &ar) == 0)
		luaL_argerror(L, 1, "invalid level");
		lua_getinfo(L, "f", &ar);
		if (lua_isnil(L, -1))
		luaL_error(L, "no function environment for tail call at level %d",
						level);
	}
}

static int l_setfenv(lua_State * L)
{
	luaL_checktype(L, 2, LUA_TTABLE);
	getfunc(L, 0);
	lua_pushvalue(L, 2);
	if (lua_isnumber(L, 1) && lua_tonumber(L, 1) == 0) {
	    /* change environment of current thread */
	    lua_pushthread(L);
	    lua_insert(L, -2);
	    lua_setfenv(L, -2);
	    return 0;
	}
	else if (lua_iscfunction(L, -2) || lua_setfenv(L, -2) == 0)
	    luaL_error(L, LUA_QL("setfenv") " cannot change environment of given object");

	return 1;
}

#if 0
static int l_debug_getinfo(lua_State * L)
{
	lua_Debug ar;
	int arg;
	lua_State *L1 = getthread(L, &arg);
	const char *options = luaL_optstring(L, arg+2, "flnStu");
	if (lua_isnumber(L, arg+1)) {
	    if (!lua_getstack(L1, (int)lua_tointeger(L, arg+1), &ar)) {
		lua_pushnil(L);  /* level out of range */
		return 1;
		}
	}
	else if (lua_isfunction(L, arg+1)) {
	    lua_pushfstring(L, ">%s", options);
	    options = lua_tostring(L, -1);
	    lua_pushvalue(L, arg+1);
	    lua_xmove(L, L1, 1);
	}
	else
	    return luaL_argerror(L, arg+1, "function or level expected");
	if (!lua_getinfo(L1, options, &ar))
	    return luaL_argerror(L, arg+2, "invalid option");
	lua_createtable(L, 0, 2);
	if (strchr(options, 'S')) {
	    settabss(L, "source", ar.source);
	    settabss(L, "short_src", ar.short_src);
	    settabsi(L, "linedefined", ar.linedefined);
	    settabsi(L, "lastlinedefined", ar.lastlinedefined);
	    settabss(L, "what", ar.what);
	}
	if (strchr(options, 'l'))
	    settabsi(L, "currentline", ar.currentline);
	if (strchr(options, 'u')) {
	    settabsi(L, "nups", ar.nups);
	    settabsi(L, "nparams", ar.nparams);
	    settabsb(L, "isvararg", ar.isvararg);
	}
	if (strchr(options, 'n')) {
	    settabss(L, "name", ar.name);
	    settabss(L, "namewhat", ar.namewhat);
	}
	if (strchr(options, 't'))
	    settabsb(L, "istailcall", ar.istailcall);
	if (strchr(options, 'L'))
	    treatstackoption(L, L1, "activelines");
	if (strchr(options, 'f'))
	    treatstackoption(L, L1, "func");
	return 1;  /* return table */
}

static int l_debug_getupvalue(lua_State * L)
{
	const char *name;
	int n = luaL_checkint(L, 2);
	luaL_checktype(L, 1, LUA_TFUNCTION);
	name = lua_getupvalue(L, 1, n);
	if (name == NULL) return 0;
	lua_pushstring(L, name);
	lua_insert(L, -2);
	return 2;
}

static int checkupval (lua_State *L, int argf, int argnup) {
	lua_Debug ar;
	int nup = luaL_checkint(L, argnup);
	luaL_checktype(L, argf, LUA_TFUNCTION);
	lua_pushvalue(L, argf);
	lua_getinfo(L, ">u", &ar);
	luaL_argcheck(L, 1 <= nup && nup <= ar.nups, argnup, "invalid upvalue index");
	return nup;
}

static int l_debug_upvaluejoin(lua_State * L)
{
	int n1 = checkupval(L, 1, 2);
	int n2 = checkupval(L, 3, 4);
	luaL_argcheck(L, !lua_iscfunction(L, 1), 1, "Lua function expected");
	luaL_argcheck(L, !lua_iscfunction(L, 3), 3, "Lua function expected");
	lua_upvaluejoin(L, 1, n1, 3, n2);
	return 0;
}
#endif

// type
static int l_type(lua_State *L)
{
	luaL_checkany(L, 1);
	lua_pushstring(L, luaL_typename(L, 1));
	return 1;
}

// tostring
static int l_tostring (lua_State *L)
{
	luaL_checkany(L, 1);
	lua_tolstring(L, 1, NULL);
	return 1;
}

// setmetatable
static int l_setmetatable(lua_State * L)
{
	int t = lua_type(L, 2);
	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_argcheck(L, t == LUA_TNIL || t == LUA_TTABLE, 2,
						"nil or table expected");
	if (luaL_getmetafield(L, 1, "__metatable"))
	    return luaL_error(L, "cannot change a protected metatable");
	lua_settop(L, 2);
	lua_setmetatable(L, 1);
	return 1;
}

static int l_getmetatable (lua_State *L)
{
	luaL_checkany(L, 1);
	if (!lua_getmetatable(L, 1))
	{
		lua_pushnil(L);
		return 1;  /* no metatable */
	}
	luaL_getmetafield(L, 1, "__metatable");
	return 1;  /* returns either __metatable field (if present) or metatable */
}

// more ipairs
static int l_ipairsaux(lua_State * L)
{
	int i = luaL_checkint(L, 2);
	luaL_checktype(L, 1, LUA_TTABLE);
	i++;
	lua_pushinteger(L, i);
	lua_rawgeti(L, 1, i);

	return lua_isnil(L, -1) ? 1 : 2;
}

// ipairs
static int l_ipairs(lua_State * L)
{
	if(!luaL_getmetafield(L, 1, "__ipairs"))
	{
		luaL_checktype(L, 1, LUA_TTABLE);
		lua_pushcfunction(L, l_ipairsaux);
		lua_pushvalue(L, 1);
		lua_pushinteger(L, 0);
	}
	else
	{
		lua_pushvalue(L, 1);
		lua_call(L, 1, 3);
	}

	return 3;
}

// String substring
// strsub (str, start, length)
static int l_strsub(lua_State * L)
{
	size_t l;
	const char * c = luaL_checklstring(L, 1, &l);
	int start = luaL_checkinteger(L, 2) - 1;
	size_t length = luaL_checkinteger(L, 3);

	if(!c || length <= 0 || (size_t)start >= l)
		return 0;

	if(length > l - start)
		length = l - start;

	lua_pushlstring(L, c + start, length);
	return 1;
}

// String char
// strchar (str, index)
static int l_strchar(lua_State * L)
{
	const char * c = luaL_checkstring(L, 1);
	int index = luaL_checkinteger(L, 2) - 1;
	
	if(c == NULL)
		return 0;

	lua_pushlstring(L, c + index, 1);
	return 1;
}

// String length
// strlen (str)
static int l_strlen(lua_State * L)
{
	const char * c = luaL_checkstring(L, 1);

	lua_pushinteger(L, strlen(c));
	return 1;
}

// Debug print
// print (...)
static int l_print(lua_State * L)
{
	char buf[0x200];
	int n = lua_gettop(L);
	int i;

	buf[0] = 0;

	lua_getglobal(L, "tostring");
	for(i = 1;i <= n;i++)
	{
		const char * s;
		size_t l;

		lua_pushvalue(L, -1);
		lua_pushvalue(L, i);
		lua_call(L, 1, 1);
		s = lua_tolstring(L, -1, &l);
		if(s == NULL)
		{
			lua_pushstring(L, "'tostring' must return a string to 'print'");
			LUA_ERROR(L);
			lua_pop(L, 1);
		}

		if(i > 1)
			strcat(buf, "\t");
		strcat(buf, s);

		lua_pop(L, 1);
	}

	dbgOut("%s", buf);
	return 0;
}

// Create a script thread
// createThread (threadFunc, ...)
static int l_createThread(lua_State * L)
{
	int i, nargs;
	lua_State * thread;
	CLuaManager * l = CLuaManager::singleton;
	if(lua_gettop(L) < 2)
		dbgError("createThread called with %i arguments, expected at least 2");
	
	nargs = lua_gettop(L) - 1;
	thread = l->CreateThread(nargs);

	// Push the function onto the new thread's stack
	lua_pushvalue(L, 2);
	lua_xmove(L, thread, 1);

	// Push the arguments onto the stack
	lua_pushvalue(L, 1);
	lua_xmove(L, thread, 1);

	for(i = 1;i < nargs;i++)
	{
		lua_pushvalue(L, i + 2);
		lua_xmove(L, thread, 1);
	}

	return 0;
}

// Delay the current thread
// wait (seconds)
static int l_wait(lua_State * L)
{
	CLuaManager * l = CLuaManager::singleton;
	std::vector<LUA_THREAD>::iterator i;
	std::vector<LUA_THREAD> * v;
	int index;

	if(lua_gettop(L) != 1)
		dbgError("wait called with %i arguments, expected 0", lua_gettop(L));

	index = l->FindThread(L, &v);
	v->at(index).waittime = luaL_checknumber(L, 1);

	// Wait causes the coroutine to yield
	return lua_yield(L, 0);
}

// Yield execution of the current thread for this frame
// yield ()
static int l_yield(lua_State * L)
{
	return lua_yield(L, 0);
}

// Delay the current thread until an event is triggered
// waittill(entity, eventName)
static int l_waittill(lua_State * L)
{
	CLuaManager * l = CLuaManager::singleton;
	std::vector<LUA_THREAD> * v;
	int i = l->FindThread(L, &v);
	const char * c = luaL_checkstring(L, 2);

	// Set the wait event
	v->at(i).waitEvent.entity = lua_touserdata(L, 1);
	if(c)
	{
		if(strlen(c) >= ENTITY_EVENT_NAME_LENGTH)
			dbgError("event name '%s' is too long, max length is %i characters", c, ENTITY_EVENT_NAME_LENGTH - 1);
		else
			strcpy(v->at(i).waitEvent.event, c);
	}

	// This waits, so yield
	return lua_yield(L, 0);
}

// Mark this function to end on this event
// endon(entity, eventName)
static int l_endon(lua_State * L)
{
	CLuaManager * l = CLuaManager::singleton;
	std::vector<LUA_THREAD> * v;
	bool foundEvent = false;
	int i = l->FindThread(L, &v), j;
	const char * c = luaL_checkstring(L, 2);

	// Find an open event
	for(j = 0;j < ENDON_EVENT_COUNT;j++)
	{
		if(v->at(i).endonEvents[j].entity)
			continue;

		foundEvent = true;
		v->at(i).endonEvents[j].entity = lua_touserdata(L, 1);
		if(c)
		{
			if(strlen(c) >= ENTITY_EVENT_NAME_LENGTH)
				dbgError("event name '%s' is too long, max length is %i characters", c, ENTITY_EVENT_NAME_LENGTH - 1);
			else
				strcpy(v->at(i).endonEvents[j].event, c);
		}

		break;
	}

	return 0;
}

// Create a callback
// onnotify(entity, eventName, callbackFunction)
static int l_onnotify(lua_State * L)
{
	CLuaManager * l = CLuaManager::singleton;
	void * entity = lua_touserdata(L, 1);
	const char * eventName = luaL_checkstring(L, 2);
	int callbackFunction, i;

	lua_pushvalue(L, 3);
	callbackFunction = luaL_ref(L, LUA_REGISTRYINDEX);

	// Find an empty slot
	for(i = 0;i < ONNOTIFY_EVENT_COUNT;i++)
	{
		if(l->onnotify[i].evt.entity)
			continue;

		l->onnotify[i].evt.entity = entity;
		l->onnotify[i].funcReference = callbackFunction;

		if(strlen(eventName) >= ENTITY_EVENT_NAME_LENGTH)
			dbgError("event name '%s' is too long, max length is %i characters", eventName, ENTITY_EVENT_NAME_LENGTH - 1);
		else
			strcpy(l->onnotify[i].evt.event, eventName);

		break;
	}

	return 0;
}

// Trigger an event
// notify(entity, eventName)
static int l_notify(lua_State * L)
{
	CLuaManager * l = CLuaManager::singleton;
	void * waitEntity = lua_touserdata(L, 1);
	const char * waitEvent = luaL_checkstring(L, 2);
	void * argument;
	
	// optional argument
	if(lua_islightuserdata(L, 3))
		argument = lua_touserdata(L, 3);
	else
		argument = waitEntity;

	l->Notify(waitEntity, waitEvent, argument);

	return 0;
}

// Var stuff

// varSetString(varName, varValue)
static int l_varSetString(lua_State * L)
{
	CVar * var = CVar::Find(luaL_checkstring(L, 1));
	var->Set(luaL_checkstring(L, 2));

	return 0;
}

// varSetDouble(varName, varValue)
static int l_varSetDouble(lua_State * L)
{
	CVar * var = CVar::Find(luaL_checkstring(L, 1));
	var->Set(luaL_checknumber(L, 2));
	
	return 0;
}

// varSetInt(varName, varValue)
static int l_varSetInt(lua_State * L)
{
	CVar * var = CVar::Find(luaL_checkstring(L, 1));
	var->Set((int)luaL_checknumber(L, 2));

	return 0;
}

// varSetBool(varName, varValue)
static int l_varSetBool(lua_State * L)
{
	luaL_checkany(L, 2);

	CVar * var = CVar::Find(luaL_checkstring(L, 1));
	var->Set(lua_toboolean(L, 2) ? "true" : "false");

	return 0;
}

// This determines the function to call depending on the type
// varSet(varName, varValue)
static int l_varSet(lua_State * L)
{
	if(lua_isboolean(L, 2))
		l_varSetBool(L);
	else if(lua_isnumber(L, 2))
		l_varSetDouble(L);
	else if(lua_isstring(L, 2))
		l_varSetString(L);
	else
		luaL_error(L, "unknown value type");

	return 0;
}

// Resets a var to the default value
// varReset(varName)
static int l_varReset(lua_State * L)
{
	CVar * var = CVar::Find(luaL_checkstring(L, 1));
	var->Reset();

	return 0;
}

// varGetString(varName)
static int l_varGetString(lua_State * L)
{
	CVar * var = CVar::Find(luaL_checkstring(L, 1));

	if(!var)
		lua_pushnil(L);
	else
		lua_pushstring(L, var->GetString());

	return 1;
}

// varGetDouble(varName)
static int l_varGetDouble(lua_State * L)
{
	CVar * var = CVar::Find(luaL_checkstring(L, 1));

	if(!var)
		lua_pushnumber(L, 0);
	else
		lua_pushnumber(L, var->GetDouble());

	return 1;
}

// varGetInt(varName)
static int l_varGetInt(lua_State * L)
{
	CVar * var = CVar::Find(luaL_checkstring(L, 1));

	if(!var)
		lua_pushnumber(L, 0);
	else
		lua_pushnumber(L, var->GetInt());

	return 1;
}

// varGetBool(varName)
static int l_varGetBool(lua_State * L)
{
	CVar * var = CVar::Find(luaL_checkstring(L, 1));

	if(!var)
		lua_pushboolean(L, 0);
	else
		lua_pushboolean(L, var->GetBool());

	return 1;
}

// Testing
// test(modelName, x, y, z)
static int l_test(lua_State * L)
{
	const char * modelName;
	double x, y, z;

	modelName = luaL_checkstring(L, 1);
	x = luaL_checknumber(L, 2);
	y = luaL_checknumber(L, 3);
	z = luaL_checknumber(L, 4);

	Ogre::SceneManager *mgr = GameApplication::singleton->mSceneMgr;
	mgr->
		getRootSceneNode()->
		createChildSceneNode(
			Ogre::Vector3(x, y, z)
		)->
		attachObject(
			mgr->createEntity(modelName)
		);

	return 0;
}

static void l_scripttotable_helper(lua_State * L, ConfigNode * node)
{
	// Recursive function
	// We populate the table at stack -1 with the children in 'node'
	// Duplicate values are allowed, but will replace any previous values
	uint i;
	std::vector<ConfigNode*>& children = node->getChildren();

	// Walk through the children, setting the tables for those with children
	// Setting the values for those without
	for(i = 0;i < children.size();i++)
	{
		// push the key
		lua_pushstring(L, children[i]->getName().c_str());

		// Check for children
		if(children[i]->getChildren().size() != 0)
		{
			// create and push the value as a table
			lua_createtable(L, 0, 0);

			// add the children's values to the table
			l_scripttotable_helper(L, children[i]);
		}
		else
		{
			// push the value of this child
			if(children[i]->getValues().size() == 0)
				lua_pushstring(L, "");
			else
				lua_pushstring(L, children[i]->getValue().c_str());
		}

		// t[k] = v
		lua_settable(L, -3);
	}
}

static int l_scripttotable(lua_State * L)
{
	const char * scriptType, * scriptName;
	ConfigScriptLoader& loader = ConfigScriptLoader::getSingleton();
	ConfigNode * node;

	scriptType = luaL_checkstring(L, 1);
	scriptName = luaL_checkstring(L, 2);

	node = loader.getConfigScript(scriptType, scriptName);
	if(node == NULL)
		lua_pushnil(L);
	else
	{
		// Push a new table and populate it
		lua_createtable(L, 0, 0);
		l_scripttotable_helper(L, node);

		// The table is on the stack, so it will be returned
	}

	return 1;
}

static int l_next(lua_State * L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_settop(L, 2);  /* create a 2nd argument if there isn't one */
	if (lua_next(L, 1))
		return 2;
	else
	{
		lua_pushnil(L);
		return 1;
	}
}

void _addLuaFunction(lua_CFunction func, const char * name)
{
	CLuaManager::singleton->addLuaFunction(func, name);
}

void CLuaManager::setupLuaFunctions()
{
	// Testing
	addLuaFunction(l_test, "test");

	// Utility
	addLuaFunction(l_setfenv, "setfenv");
	//addLuaFunction(l_debug_getinfo, "debug_getinfo");
	//addLuaFunction(l_debug_getupvalue, "debug_getupvalue");
	//addLuaFunction(l_debug_upvaluejoin, "debug_upvaluejoin");
	addLuaFunction(l_type, "type");
	addLuaFunction(l_tostring, "tostring");
	addLuaFunction(l_setmetatable, "setmetatable");
	addLuaFunction(l_getmetatable, "getmetatable");
	addLuaFunction(l_ipairsaux, "ipairsaux");
	addLuaFunction(l_ipairs, "ipairs");
	addLuaFunction(l_strsub, "strsub");
	addLuaFunction(l_strchar, "strchar");
	addLuaFunction(l_strlen, "strlen");
	addLuaFunction(l_print, "print");

	// Threading
	addLuaFunction(l_createThread, "createThread");
	addLuaFunction(l_wait, "wait");
	addLuaFunction(l_yield, "yield");
	addLuaFunction(l_waittill, "waittill");
	addLuaFunction(l_endon, "endon");
	addLuaFunction(l_onnotify, "onnotify");
	addLuaFunction(l_notify, "notify");
	
	// Vars
	addLuaFunction(l_varSetString, "varSetString");
	addLuaFunction(l_varSetDouble, "varSetDouble");
	addLuaFunction(l_varSetInt, "varSetInt");
	addLuaFunction(l_varSetBool, "varSetBool");
	addLuaFunction(l_varSet, "varSet");
	addLuaFunction(l_varReset, "varReset");
	addLuaFunction(l_varGetString, "varGetString");
	addLuaFunction(l_varGetDouble, "varGetDouble");
	addLuaFunction(l_varGetInt, "varGetInt");
	addLuaFunction(l_varGetBool, "varGetBool");

	// Scripts
	addLuaFunction(l_next, "next");
	addLuaFunction(l_scripttotable, "scripttotable");

	// Gorilla
	//RegisterGorilla(_addLuaFunction, L);
}

void * CLuaManager::getNewThreadId()
{
	int id = 1;
	void * vid;
	uint i;

	for(;;)
	{
		if(id == -1)
			dbgError("no unique thread ids left");

		vid = (void *)id;

		for(i = 0;i < luaThreads.size();i++)
		{
			if(luaThreads[i].id == vid)
			{
				id++;
				continue;
			}
		}

		for(i = 0;i < queueThreads.size();i++)
		{
			if(queueThreads[i].id == vid)
			{
				id++;
				continue;
			}
		}

		for(i = 0;i < tempQueue.size();i++)
		{
			if(tempQueue[i].id == vid)
			{
				id++;
				continue;
			}
		}

		break;
	}

	return vid;
}

std::vector<LUA_THREAD>::iterator CLuaManager::tickThread(std::vector<LUA_THREAD>::iterator i, std::vector<LUA_THREAD> * v, double delta, const char * Event)
{
	if(Event)
	{
		// Only run coroutines that are waiting on this event
		if(i->waitEvent.entity && _stricmp(i->waitEvent.event, Event) == 0)
		{
			i->waitEvent.entity = NULL;
		}
		else
		{
			// Skip
			i++;
			return i;
		}
	}

	// Allow coroutines to sleep
	if(i->waittime > 0)
	{
		i->waittime -= delta;
			
		// If the time has not elapsed yet, skip over this coroutine
		if(i->waittime > 0)
		{
			i++;
			return i;
		}
	}

	// Allow coroutines to wait on an event
	if(i->waitEvent.entity)
	{
		i++;
		return i;
	}

	// Resume the coroutine
	int r = 0;
	if(!i->terminate && (i->firstRun || (lua_status(i->L) == LUA_YIELD)))
	{
		r = lua_resume(i->L, i->firstRun ? i->nargs : 0);
		i->firstRun = false;
	}
	
	// Check to see if execution has halted
	if(r == LUA_YIELD)
		i++;
	else if(r == 0)
	{
		// _G.ThreadList[L] = nil
		lua_getglobal(i->L, "ThreadList");
		lua_pushthread(i->L);
		lua_pushnil(i->L);
		lua_settable(i->L, -3);

		// _G.ThreadIDs[L] = nil
		lua_getglobal(i->L, "ThreadIDs");
		lua_pushthread(i->L);
		lua_pushnil(i->L);
		lua_settable(i->L, -3);

		i = v->erase(i);
	}
	else
	{
		// Uh oh, error!

		// Only complain if the coroutine wasn't just scheduled for termination
		if(!i->terminate)
			LUA_ERROR(i->L);

		// _G.ThreadList[L] = nil
		lua_getglobal(i->L, "ThreadList");
		lua_pushthread(i->L);
		lua_pushnil(i->L);
		lua_settable(i->L, -3);
		
		i = v->erase(i);
	}

	return i;
}