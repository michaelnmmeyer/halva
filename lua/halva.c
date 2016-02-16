#include <lua.h>
#include <lauxlib.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include "../halva.h"

/* Begin compatibility code for Lua 5.1. Copy-pasted from Lua's source. */
#if !defined(LUA_VERSION_NUM) || LUA_VERSION_NUM == 501

static void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkstack(L, nup+1, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    int i;
    lua_pushstring(L, l->name);
    for (i = 0; i < nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -(nup+1));
    lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    lua_settable(L, -(nup + 3));
  }
  lua_pop(L, nup);  /* remove upvalues */
}

#define luaL_newlibtable(L,l) \
  lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define luaL_newlib(L,l)   (luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))

#endif
/* End compatibility code. */

#define HV_MT "halva"
#define HV_ENC_MT "halva.enc"
#define HV_ITER_MT "halva.iter"

static int hv_lua_enc_new(lua_State *lua)
{
   struct halva_enc *enc = lua_newuserdata(lua, sizeof *enc);
   *enc = (struct halva_enc)HV_ENC_INIT;
   luaL_getmetatable(lua, HV_ENC_MT);
   lua_setmetatable(lua, -2);
   return 1;
}

static int hv_lua_enc_add(lua_State *lua)
{
   struct halva_enc *enc = luaL_checkudata(lua, 1, HV_ENC_MT);
   size_t len;
   const void *word = luaL_checklstring(lua, 2, &len);

   int ret = hv_enc_add(enc, word, len);
   if (ret) {
      /* Programming error. */
      lua_pushstring(lua, hv_strerror(ret));
      return lua_error(lua);
   }
   return 0;
}

static int hv_lua_enc_dump(lua_State *lua)
{
   struct halva_enc *enc = luaL_checkudata(lua, 1, HV_ENC_MT);
   const char *path = luaL_checkstring(lua, 2);

   FILE *fp = fopen(path, "wb");
   if (!fp) {
      lua_pushnil(lua);
      lua_pushstring(lua, strerror(errno));
      return 2;
   }

   int ret = hv_enc_dump_file(enc, fp);
   switch (ret) {
   case HV_OK:
      break;
   case HV_EIO:
      lua_pushnil(lua);
      lua_pushstring(lua, strerror(errno));
      return 2;
   default:
      /* Programming error. */
      lua_pushstring(lua, hv_strerror(ret));
      return lua_error(lua);
   }

   if (fclose(fp)) {
      lua_pushnil(lua);
      lua_pushstring(lua, strerror(errno));
      return 2;
   }
   lua_pushboolean(lua, 1);
   return 1;
}

static int hv_lua_enc_clear(lua_State *lua)
{
   struct halva_enc *enc = luaL_checkudata(lua, 1, HV_ENC_MT);
   hv_enc_clear(enc);
   return 0;
}

static int hv_lua_enc_free(lua_State *lua)
{
   struct halva_enc *enc = luaL_checkudata(lua, 1, HV_ENC_MT);
   hv_enc_fini(enc);
   return 0;
}

struct halva_lua {
   struct halva *hv;
   int lua_ref;
   int ref_cnt;
};

static int hv_lua_load(lua_State *lua)
{
   const char *path = luaL_checkstring(lua, 1);
   struct halva_lua *hv = lua_newuserdata(lua, sizeof *hv);

   FILE *fp = fopen(path, "rb");
   if (!fp) {
      lua_pushnil(lua);
      lua_pushstring(lua, strerror(errno));
      return 2;
   }

   int ret = hv_load_file(&hv->hv, fp);
   fclose(fp);
   if (ret) {
      lua_pushnil(lua);
      lua_pushstring(lua, hv_strerror(ret));
      return 2;
   }

   hv->lua_ref = LUA_NOREF;
   hv->ref_cnt = 0;
   luaL_getmetatable(lua, HV_MT);
   lua_setmetatable(lua, -2);
   return 1;
}

static int hv_lua_free(lua_State *lua)
{
   struct halva_lua *hv = luaL_checkudata(lua, 1, HV_MT);
   hv_free(hv->hv);
   assert(hv->ref_cnt == 0 && hv->lua_ref == LUA_NOREF);
   return 0;
}

static const struct halva *check_hv(lua_State *lua)
{
   const struct halva_lua *hv = luaL_checkudata(lua, 1, HV_MT);
   return hv->hv;
}

static uint32_t hv_abs_index(lua_State *lua, int idx, const struct halva *hv)
{
   int64_t num = luaL_checknumber(lua, idx);
   if (num < 0) {
      num += hv_size(hv) + 1;
      if (num < 0)
         num = 0;
   }
   return num;
}

static int hv_lua_extract(lua_State *lua)
{
   const struct halva *hv = check_hv(lua);
   uint32_t pos = hv_abs_index(lua, 2, hv);

   char word[HV_MAX_WORD_LEN + 1];
   size_t len = hv_extract(hv, pos, word);
   if (len)
      lua_pushlstring(lua, word, len);
   else
      lua_pushnil(lua);
   return 1;
}

static int hv_lua_locate(lua_State *lua)
{
   const struct halva *hv = check_hv(lua);
   size_t len;
   const char *word = luaL_checklstring(lua, 2, &len);

   uint32_t pos = hv_locate(hv, word, len);
   if (pos)
      lua_pushnumber(lua, pos);
   else
      lua_pushnil(lua);
   return 1;
}

static int hv_lua_size(lua_State *lua)
{
   const struct halva *hv = check_hv(lua);
   lua_pushnumber(lua, hv_size(hv));
   return 1;
}

struct halva_lua_iter {
   struct halva_iter it;
   struct halva_lua *hv;
};

static int hv_lua_iter_next(lua_State *lua);

static struct halva_iter *hv_lua_iter_new(lua_State *lua, struct halva **hvp)
{
   struct halva_lua *hv = luaL_checkudata(lua, 1, HV_MT);
   struct halva_lua_iter *it = lua_newuserdata(lua, sizeof *it);

   if (hv->ref_cnt++ == 0) {
      lua_pushvalue(lua, 1);
      hv->lua_ref = luaL_ref(lua, LUA_REGISTRYINDEX);
   }
   it->hv = hv;

   luaL_getmetatable(lua, HV_ITER_MT);
   lua_setmetatable(lua, -2);

   *hvp = hv->hv;
   return &it->it;
}

static int hv_lua_iter_init(lua_State *lua)
{
   lua_pushnil(lua);

   struct halva *hv;
   struct halva_iter *it = hv_lua_iter_new(lua, &hv);

   uint32_t pos;
   switch (lua_type(lua, 2)) {
   case LUA_TNUMBER: {
      uint32_t num = hv_abs_index(lua, 2, hv);
      pos = hv_iter_initn(it, hv, num);
      break;
   }
   case LUA_TSTRING: {
      size_t len;
      const char *str = lua_tolstring(lua, 2, &len);
      pos = hv_iter_inits(it, hv, str, len);
      break;
   }
   case LUA_TNIL:
   case LUA_TNONE:
      pos = hv_iter_init(it, hv);
      break;
   default: {
      const char *type = lua_typename(lua, lua_type(lua, 2));
      return luaL_error(lua, "bad value at #2 (expect string, number, or nil, have %s)", type);
   }
   }

   lua_pushcclosure(lua, hv_lua_iter_next, 1);
   if (pos)
      lua_pushnumber(lua, pos);
   else
      lua_pushnil(lua);
   return 2;
}

static int hv_lua_iter_next(lua_State *lua)
{
   struct halva_iter *it = lua_touserdata(lua, lua_upvalueindex(1));
   size_t len;
   const char *word = hv_iter_next(it, &len);
   if (word) {
      lua_pushlstring(lua, word, len);
      return 1;
   }
   return 0;
}

static int hv_lua_iter_fini(lua_State *lua)
{
   struct halva_lua_iter *it = luaL_checkudata(lua, 1, HV_ITER_MT);
   struct halva_lua *hv = it->hv;
   if (--hv->ref_cnt == 0) {
      luaL_unref(lua, LUA_REGISTRYINDEX, hv->lua_ref);
      hv->lua_ref = LUA_NOREF;
   }
   return 0;
}

int luaopen_halva(lua_State *lua)
{
   const luaL_Reg enc_fns[] = {
      {"__gc", hv_lua_enc_free},
      {"add", hv_lua_enc_add},
      {"clear", hv_lua_enc_clear},
      {"dump", hv_lua_enc_dump},
      {NULL, NULL},
   };
   luaL_newmetatable(lua, HV_ENC_MT);
   lua_pushvalue(lua, -1);
   lua_setfield(lua, -2, "__index");
   luaL_setfuncs(lua, enc_fns, 0);

   const luaL_Reg fns[] = {
      {"__gc", hv_lua_free},
      {"__len", hv_lua_size},
      {"locate", hv_lua_locate},
      {"extract", hv_lua_extract},
      {"size", hv_lua_size},
      {"iter", hv_lua_iter_init},
      {NULL, NULL},
   };
   luaL_newmetatable(lua, HV_MT);
   lua_pushvalue(lua, -1);
   lua_setfield(lua, -2, "__index");
   luaL_setfuncs(lua, fns, 0);

   luaL_newmetatable(lua, HV_ITER_MT);
   lua_pushliteral(lua, "__gc");
   lua_pushcfunction(lua, hv_lua_iter_fini);
   lua_settable(lua, -3);

   const luaL_Reg lib[] = {
      {"encoder", hv_lua_enc_new},
      {"load", hv_lua_load},
      {NULL, NULL},
   };
   luaL_newlib(lua, lib);
   lua_pushnumber(lua, HV_MAX_WORD_LEN);
   lua_setfield(lua, -2, "MAX_WORD_LEN");

   lua_pushstring(lua, HV_VERSION);
   lua_setfield(lua, -2, "VERSION");

   return 1;
}
