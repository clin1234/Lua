/*
** $Id: lbaselib.c,v 1.74 2002/05/16 18:39:46 roberto Exp roberto $
** Basic library
** See Copyright Notice in lua.h
*/



#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "luadebug.h"
#include "lualib.h"




/*
** If your system does not support `stdout', you can just remove this function.
** If you need, you can define your own `print' function, following this
** model but changing `fputs' to put the strings at a proper place
** (a console window or a log file, for instance).
*/
static int luaB_print (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int i;
  lua_getglobal(L, "tostring");
  for (i=1; i<=n; i++) {
    const char *s;
    lua_pushvalue(L, -1);  /* function to be called */
    lua_pushvalue(L, i);   /* value to print */
    lua_rawcall(L, 1, 1);
    s = lua_tostring(L, -1);  /* get result */
    if (s == NULL)
      return luaL_verror(L, "`tostring' must return a string to `print'");
    if (i>1) fputs("\t", stdout);
    fputs(s, stdout);
    lua_pop(L, 1);  /* pop result */
  }
  fputs("\n", stdout);
  return 0;
}


static int luaB_tonumber (lua_State *L) {
  int base = luaL_opt_int(L, 2, 10);
  if (base == 10) {  /* standard conversion */
    luaL_check_any(L, 1);
    if (lua_isnumber(L, 1)) {
      lua_pushnumber(L, lua_tonumber(L, 1));
      return 1;
    }
  }
  else {
    const char *s1 = luaL_check_string(L, 1);
    char *s2;
    unsigned long n;
    luaL_arg_check(L, 2 <= base && base <= 36, 2, "base out of range");
    n = strtoul(s1, &s2, base);
    if (s1 != s2) {  /* at least one valid digit? */
      while (isspace((unsigned char)(*s2))) s2++;  /* skip trailing spaces */
      if (*s2 == '\0') {  /* no invalid trailing characters? */
        lua_pushnumber(L, n);
        return 1;
      }
    }
  }
  lua_pushnil(L);  /* else not a number */
  return 1;
}


static int luaB_error (lua_State *L) {
  lua_settop(L, 1);
  return lua_errorobj(L);
}


static int luaB_metatable (lua_State *L) {
  luaL_check_any(L, 1);
  if (lua_isnone(L, 2)) {
    if (!lua_getmetatable(L, 1))
      return 0;  /* no metatable */
    else {
      lua_pushliteral(L, "__metatable");
      lua_rawget(L, -2);
      if (lua_isnil(L, -1))
        lua_pop(L, 1);
    }
  }
  else {
    int t = lua_type(L, 2);
    luaL_check_type(L, 1, LUA_TTABLE);
    luaL_arg_check(L, t == LUA_TNIL || t == LUA_TTABLE, 2, "nil/table expected");
    lua_settop(L, 2);
    lua_setmetatable(L, 1);
  }
  return 1;
}


static int luaB_globals (lua_State *L) {
  lua_getglobals(L);  /* value to be returned */
  if (!lua_isnone(L, 1)) {
    luaL_check_type(L, 1, LUA_TTABLE);
    lua_pushvalue(L, 1);  /* new table of globals */
    lua_setglobals(L);
  }
  return 1;
}

static int luaB_rawget (lua_State *L) {
  luaL_check_type(L, 1, LUA_TTABLE);
  luaL_check_any(L, 2);
  lua_rawget(L, 1);
  return 1;
}

static int luaB_rawset (lua_State *L) {
  luaL_check_type(L, 1, LUA_TTABLE);
  luaL_check_any(L, 2);
  luaL_check_any(L, 3);
  lua_rawset(L, 1);
  return 1;
}


static int luaB_gcinfo (lua_State *L) {
  lua_pushnumber(L, lua_getgccount(L));
  lua_pushnumber(L, lua_getgcthreshold(L));
  return 2;
}


static int luaB_collectgarbage (lua_State *L) {
  lua_setgcthreshold(L, luaL_opt_int(L, 1, 0));
  return 0;
}


static int luaB_type (lua_State *L) {
  luaL_check_any(L, 1);
  lua_pushstring(L, lua_typename(L, lua_type(L, 1)));
  return 1;
}


static int luaB_next (lua_State *L) {
  luaL_check_type(L, 1, LUA_TTABLE);
  lua_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (lua_next(L, 1))
    return 2;
  else {
    lua_pushnil(L);
    return 1;
  }
}


static int luaB_nexti (lua_State *L) {
  lua_Number i = lua_tonumber(L, 2);
  luaL_check_type(L, 1, LUA_TTABLE);
  if (i == 0 && lua_isnull(L, 2)) {  /* `for' start? */
    lua_getglobal(L, "nexti");  /* return generator, */
    lua_pushvalue(L, 1);  /* state, */
    lua_pushnumber(L, 0);  /* and initial value */
    return 3;
  }
  else {  /* `for' step */
    i++;  /* next value */
    lua_pushnumber(L, i);
    lua_rawgeti(L, 1, i);
    return (lua_isnil(L, -1)) ? 0 : 2;
  }
}


static int passresults (lua_State *L, int status) {
  if (status == 0) return 1;
  else {
    lua_pushnil(L);
    lua_insert(L, -2);
    return 2;
  }
}


static int luaB_loadstring (lua_State *L) {
  size_t l;
  const char *s = luaL_check_lstr(L, 1, &l);
  const char *chunkname = luaL_opt_string(L, 2, s);
  return passresults(L, lua_loadbuffer(L, s, l, chunkname));
}


static int luaB_loadfile (lua_State *L) {
  const char *fname = luaL_opt_string(L, 1, NULL);
  return passresults(L, lua_loadfile(L, fname));
}


static int luaB_assert (lua_State *L) {
  luaL_check_any(L, 1);
  if (!lua_toboolean(L, 1))
    return luaL_verror(L, "assertion failed!  %s", luaL_opt_string(L, 2, ""));
  lua_settop(L, 1);
  return 1;
}


static int luaB_unpack (lua_State *L) {
  int n, i;
  luaL_check_type(L, 1, LUA_TTABLE);
  n = lua_getn(L, 1);
  luaL_check_stack(L, n+LUA_MINSTACK, "table too big to unpack");
  for (i=1; i<=n; i++)  /* push arg[1...n] */
    lua_rawgeti(L, 1, i);
  return n;
}


static int luaB_pcall (lua_State *L) {
  int status;
  luaL_check_any(L, 1);
  luaL_check_any(L, 2);
  status = lua_pcall(L, lua_gettop(L) - 2, LUA_MULTRET, 1);
  if (status != 0)
    return passresults(L, status);
  else {
    lua_pushboolean(L, 1);
    lua_replace(L, 1);
    return lua_gettop(L);  /* return `true' + all results */
  }
}


static int luaB_tostring (lua_State *L) {
  char buff[64];
  luaL_checkany(L, 1);
  if (luaL_callmeta(L, 1, "__tostring"))  /* is there a metafield? */
    return 1;  /* use its value */
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER:
      lua_pushstring(L, lua_tostring(L, 1));
      return 1;
    case LUA_TSTRING:
      lua_pushvalue(L, 1);
      return 1;
    case LUA_TBOOLEAN:
      lua_pushstring(L, (lua_toboolean(L, 1) ? "true" : "false"));
      return 1;
    case LUA_TTABLE:
      sprintf(buff, "table: %p", lua_topointer(L, 1));
      break;
    case LUA_TFUNCTION:
      sprintf(buff, "function: %p", lua_topointer(L, 1));
      break;
    case LUA_TUSERDATA:
    case LUA_TUDATAVAL:
      sprintf(buff, "userdata: %p", lua_touserdata(L, 1));
      break;
    case LUA_TNIL:
      lua_pushliteral(L, "nil");
      return 1;
  }
  lua_pushstring(L, buff);
  return 1;
}



/*
** {======================================================
** `require' function
** =======================================================
*/


/* name of global that holds table with loaded packages */
#define REQTAB		"_LOADED"

/* name of global that holds the search path for packages */
#define LUA_PATH	"LUA_PATH"

#ifndef LUA_PATH_SEP
#define LUA_PATH_SEP	';'
#endif

#ifndef LUA_PATH_DEFAULT
#define LUA_PATH_DEFAULT	"?.lua"
#endif


static const char *getpath (lua_State *L) {
  const char *path;
  lua_getglobal(L, LUA_PATH);  /* try global variable */
  path = lua_tostring(L, -1);
  lua_pop(L, 1);
  if (path) return path;
  path = getenv(LUA_PATH);  /* else try environment variable */
  if (path) return path;
  return LUA_PATH_DEFAULT;  /* else use default */
}


static const char *pushnextpath (lua_State *L, const char *path) {
  const char *l;
  if (*path == '\0') return NULL;  /* no more pathes */
  if (*path == LUA_PATH_SEP) path++;  /* skip separator */
  l = strchr(path, LUA_PATH_SEP);  /* find next separator */
  if (l == NULL) l = path+strlen(path);
  lua_pushlstring(L, path, l - path);  /* directory name */
  return l;
}


static void pushcomposename (lua_State *L) {
  const char *path = lua_tostring(L, -1);
  const char *wild = strchr(path, '?');
  if (wild == NULL) return;  /* no wild char; path is the file name */
  lua_pushlstring(L, path, wild - path);
  lua_pushvalue(L, 1);  /* package name */
  lua_pushstring(L, wild + 1);
  lua_concat(L, 3);
}


static int luaB_require (lua_State *L) {
  const char *path;
  int status = LUA_ERRFILE;  /* not found (yet) */
  luaL_check_string(L, 1);
  lua_settop(L, 1);
  lua_pushvalue(L, 1);
  lua_setglobal(L, "_REQUIREDNAME");
  lua_getglobal(L, REQTAB);
  if (!lua_istable(L, 2)) return luaL_verror(L, REQTAB " is not a table");
  path = getpath(L);
  lua_pushvalue(L, 1);  /* check package's name in book-keeping table */
  lua_gettable(L, 2);
  if (!lua_isnil(L, -1))  /* is it there? */
    return 0;  /* package is already loaded */
  else {  /* must load it */
    while (status == LUA_ERRFILE) {
      lua_settop(L, 3);  /* reset stack position */
      if ((path = pushnextpath(L, path)) == NULL) break;
      pushcomposename(L);
      status = lua_loadfile(L, lua_tostring(L, -1));  /* try to load it */
    }
  }
  switch (status) {
    case 0: {
      lua_rawcall(L, 0, 0);  /* run loaded module */
      lua_pushvalue(L, 1);
      lua_pushboolean(L, 1);
      lua_settable(L, 2);  /* mark it as loaded */
      return 0;
    }
    case LUA_ERRFILE: {  /* file not found */
      return luaL_verror(L, "could not load package `%s' from path `%s'",
                            lua_tostring(L, 1), getpath(L));
    }
    default: {
      return luaL_verror(L, "error loading package\n%s", lua_tostring(L, -1));
    }
  }
}

/* }====================================================== */


static const luaL_reg base_funcs[] = {
  {"error", luaB_error},
  {"metatable", luaB_metatable},
  {"globals", luaB_globals},
  {"next", luaB_next},
  {"nexti", luaB_nexti},
  {"print", luaB_print},
  {"tonumber", luaB_tonumber},
  {"tostring", luaB_tostring},
  {"type", luaB_type},
  {"assert", luaB_assert},
  {"unpack", luaB_unpack},
  {"rawget", luaB_rawget},
  {"rawset", luaB_rawset},
  {"pcall", luaB_pcall},
  {"collectgarbage", luaB_collectgarbage},
  {"gcinfo", luaB_gcinfo},
  {"loadfile", luaB_loadfile},
  {"loadstring", luaB_loadstring},
  {"require", luaB_require},
  {NULL, NULL}
};


/*
** {======================================================
** Coroutine library
** =======================================================
*/


static int luaB_resume (lua_State *L) {
  lua_State *co = (lua_State *)lua_getfrombox(L, lua_upvalueindex(1));
  lua_settop(L, 0);
  if (lua_resume(L, co) != 0)
    return lua_errorobj(L);
  return lua_gettop(L);
}



static int gc_coroutine (lua_State *L) {
  lua_State *co = (lua_State *)lua_getfrombox(L, 1);
  lua_closethread(L, co);
  return 0;
}


static int luaB_coroutine (lua_State *L) {
  lua_State *NL;
  int ref;
  int i;
  int n = lua_gettop(L);
  luaL_arg_check(L, lua_isfunction(L, 1) && !lua_iscfunction(L, 1), 1,
    "Lua function expected");
  NL = lua_newthread(L);
  if (NL == NULL) return luaL_verror(L, "unable to create new thread");
  /* move function and arguments from L to NL */
  for (i=0; i<n; i++) {
    ref = lua_ref(L, 1);
    lua_getref(NL, ref);
    lua_insert(NL, 1);
    lua_unref(L, ref);
  }
  lua_cobegin(NL, n-1);
  lua_newpointerbox(L, NL);
  lua_pushliteral(L, "Coroutine");
  lua_rawget(L, LUA_REGISTRYINDEX);
  lua_setmetatable(L, -2);
  lua_pushcclosure(L, luaB_resume, 1);
  return 1;
}


static int luaB_yield (lua_State *L) {
  return lua_yield(L, lua_gettop(L));
}

static const luaL_reg co_funcs[] = {
  {"create", luaB_coroutine},
  {"yield", luaB_yield},
  {NULL, NULL}
};


static void co_open (lua_State *L) {
  luaL_opennamedlib(L, "co", co_funcs, 0);
  /* create metatable for coroutines */
  lua_pushliteral(L, "Coroutine");
  lua_newtable(L);
  lua_pushliteral(L, "__gc");
  lua_pushcfunction(L, gc_coroutine);
  lua_rawset(L, -3);
  lua_rawset(L, LUA_REGISTRYINDEX);
}

/* }====================================================== */



static void base_open (lua_State *L) {
  lua_pushliteral(L, "_G");
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  luaL_openlib(L, base_funcs, 0);  /* open lib into global table */
  lua_pushliteral(L, "_VERSION");
  lua_pushliteral(L, LUA_VERSION);
  lua_settable(L, -3);  /* set global _VERSION */
  lua_settable(L, -1);  /* set global _G */
}


LUALIB_API int lua_baselibopen (lua_State *L) {
  base_open(L);
  co_open(L);
  lua_newtable(L);
  lua_setglobal(L, REQTAB);
  return 0;
}

