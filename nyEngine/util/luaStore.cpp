/*
Name: luaStore.cpp
Author: Nathan LeRoux
Purpose: Saving script state
*/

#define LUA_CORE

#if 0
#include "..\include.h"
#include "LuaManager.h"

extern "C"
{
#include "..\lua\lstate.h"
}

#include "luaStore.h"

// File format is pretty simple

// A few implementation details:
// Only light c functions are supported
// Keep in mind this is designed for this engine's savegames

// Thread:
// void * GCPointer
// 
// ThreadData data

// Thread Data:
// int StackCount
// Object[] Stack

// Object:
// byte ObjectType
// ObjectData data

// Object Data:
// varies depending on type

// GC Object:
// void * GCPointer - used to match up objects when loading
// byte ObjectType
// GCData data

// GC Data:
// varies depending on type

// The string table is really just a table of string objects
// String Table:
// uint32 StringCount
// String Strings;

// String:
// void * GCPointer
// ushort Length
// char Text

// File itself:
// StringTable Strings
// int GlobalGCCount
// Object[] GlobalGCObjects
// int ThreadCount
// Thread[] Threads

void luaStore_writeTable(GCObject * obj, file& f, std::vector<void *>& functions);
void luaStore_writeObject(GCObject * obj, file& f, std::vector<void *>& functions);
void luaStore_writeValue(TValue * v, file& f, std::vector<void *>& functions);
void luaStore_writeThread(lua_State * L, file& f, std::vector<void *>& functions);

void luaStore_writeTable(GCObject * obj, file& f, std::vector<void *>& functions)
{
	// Table format:
	// int sizearray
	// sizearray objects
	f.write(obj->h.sizearray);

	int i;
	for(i = 0;i < obj->h.sizearray;i++)
		luaStore_writeValue(&obj->h.array[i], f, functions);
}

void luaStore_writeLongString(GCObject * obj, file& f)
{
	// Long String format:
	// ushort length
	// length bytes
	if(obj->ts.tsv.len > 0xFFFF)
		dbgError("string too long");
	f.write((ushort)obj->ts.tsv.len);

	f.write(&obj->ts + 1, obj->ts.tsv.len);
}

void luaStore_writeHeader(GCObject * obj, file& f)
{
	f.write(&obj, sizeof(void*));
	f.write(obj->gch.tt);
}

void luaStore_writeObject(GCObject * obj, file& f, std::vector<void *>& functions)
{
	// write the data
	switch(obj->gch.tt)
	{
	case LUA_TSHRSTR:
		dbgError("short string is not a valid object type");
		break;
	case LUA_TLNGSTR:
		luaStore_writeHeader(obj, f);
		luaStore_writeLongString(obj, f);
		break;
	case LUA_TTABLE:
		luaStore_writeHeader(obj, f);
		luaStore_writeTable(obj, f, functions);
		break;
	case LUA_TLCL:
		// Lua closure, serialize this
		luaStore_writeHeader(obj, f);
		break;
	case LUA_TTHREAD:
		// Thread, serialize this
		luaStore_writeHeader(obj, f);
		luaStore_writeThread(&obj->th, f, functions);
		break;
	case LUA_TPROTO:
		// Prototype, serialize this
		luaStore_writeHeader(obj, f);
		break;
	case LUA_TUPVAL:
		// Upval, serialize this
		luaStore_writeHeader(obj, f);
		break;
	default:
		dbgError("unknown object type %i", obj->gch.tt);
		break;
	}
}

void luaStore_writeValue(TValue * v, file& f, std::vector<void *>& functions)
{
	uint i;
	bool found;
	byte type = ttype(v);

	f.write(type);

	switch(type)
	{
	case LUA_TNIL: // nil has no data
		break;
	case LUA_TBOOLEAN:
		f.write((byte)val_(v).b);
		break;
	case LUA_TLIGHTUSERDATA: // write the pointer
		f.write(&val_(v).p, sizeof(void*));
		break;
	case LUA_TNUMBER:
		f.write(num_(v));
		break;
	case LUA_TSTRING: // write the pointer to gc objects
	case LUA_TTABLE:
	case LUA_TUSERDATA:
	case LUA_TTHREAD:
		// all gc objects will be flushed to disk later
		f.write(&val_(v).gc, sizeof(void*));
		break;
	case LUA_TLCL:
		f.write(&val_(v).gc, sizeof(void*)); // this variant is a gc object
		break;
	case LUA_TLCF: // c-function
		// look up the function in the bindings list
		found = false;
		for(i = 0;i < functions.size();i++)
		{
			if(functions[i] == val_(v).f)
			{
				// write the index as a short
				f.write((ushort)i);
				found = true;
				break;
			}
		}
		if(!found)
			dbgError("unknown light function");
		break;
	default:
		dbgError("unknown object type %i", type);
		break;
	}
}

void luaStore_writeGC(GCObject * gc, file& f, std::vector<void *>& functions)
{
	GCObject * tmp = gc;
	uint i;
	for(i = 0;gc;i++)
		gc = gc->gch.next;
	f.write(i); // list size
	gc = tmp;
	while(gc)
	{
		luaStore_writeObject(gc, f, functions);
		gc = gc->gch.next;
	}
}

void luaStore_writeThread(lua_State * L, file& f, std::vector<void *>& functions)
{
	int i;
	StkId stack;

	// Write the thread stack
	stack = L->stack;
	f.write(L->stacksize);
	for(i = 0;i < L->stacksize;i++)
	{
		luaStore_writeValue(stack, f, functions);
		stack++;
	}
}

void luaStore_writeStrings(stringtable * strt, file&f)
{
	GCObject * list;
	int i;

	// The total amount of strings
	f.write(strt->nuse);
	
	for(i = 0;i < strt->size;i++)
	{
		list = strt->hash[i];
		while(list)
		{
			// write the object pointer
			f.write(&list, sizeof(void*));

			// write the string length
			if(list->ts.tsv.len > 0xFFFF)
				dbgError("string too long");
			f.write((ushort)list->ts.tsv.len);

			// write the string
			f.write(&list->ts + 1, list->ts.tsv.len);

			// next!
			list = list->gch.next;
		}
	}
}

void luaStore_writeThreadState(LUA_THREAD * thr, file& f)
{
	uint i;
	ushort len;
	f.write(&thr->L, sizeof(void*));
	f.write((byte)thr->firstRun);
	f.write(thr->nargs);
	f.write((byte)thr->terminate);
	if(thr->waitEvent.entity)
	{
		f.write((ushort)1);
		len = strlen(thr->waitEvent.event);
		f.write(&thr->waitEvent.entity, sizeof(void*));
		f.write(len);
		f.write(thr->waitEvent.event, len);
	}
	else
		f.write((ushort)0);

	len = 0;
	for(i = 0;i < ENDON_EVENT_COUNT;i++)
	{
		if(thr->endonEvents[i].entity)
			len++;
	}

	f.write(len);
	for(i = 0;i < ENDON_EVENT_COUNT;i++)
	{
		if(thr->endonEvents[i].entity)
		{
			len = strlen(thr->endonEvents[i].event);
			f.write(&thr->endonEvents[i].entity, sizeof(void*));
			f.write(len);
			f.write(thr->endonEvents[i].event, len);
		}
	}
}

void luaStore_save(CLuaManager * l, file& f)
{
	std::vector<LUA_THREAD>::iterator i;

	// Before writing, force a full garbage collect
	lua_gc(l->L, LUA_GCCOLLECT, 0);

	// Write string table from global state
	luaStore_writeStrings(&l->L->l_G->strt, f);
	// Write global GC list
	luaStore_writeGC(l->L->l_G->allgc, f, l->functionBindings);
	
	// write all thread pointers
	// main thread
	luaStore_writeThread(l->L, f, l->functionBindings);
	// Lists
	f.write((ushort)(l->luaThreads.size() + l->queueThreads.size()));
	// first list
	for(i = l->luaThreads.begin();i != l->luaThreads.end();i++)
		luaStore_writeThreadState(&(*i), f);
	// second (queue) list
	for(i = l->queueThreads.begin();i != l->queueThreads.end();i++)
		luaStore_writeThreadState(&(*i), f);
}

void luaStore_load(lua_State * L, file& f)
{
}
#endif