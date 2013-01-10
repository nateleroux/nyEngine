#ifndef _LUASTORE_H
#define _LUASTORE_H

void luaStore_save(lua_State * L, file& f);
void luaStore_load(lua_State * L, file& f);

#endif