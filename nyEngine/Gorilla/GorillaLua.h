#ifndef _GORILLA_LUA
#define _GORILLA_LUA

typedef void (*ADDLUAFUNC)(lua_CFunction func, const char * name);
void RegisterGorilla(ADDLUAFUNC func);

#endif // _GORILLA_LUA