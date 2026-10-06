#include "engine/script.h"

#include <SDL3/SDL.h>
#include <lua.hpp>

namespace {

// Generous for per-tick gameplay logic, small enough that a runaway loop is
// cut off in well under a frame.
constexpr long long kInstructionBudget = 2'000'000;
constexpr int kHookInterval = 1000;  // instructions between budget checks

// The VM's budget counter is reachable from the lua_State through Lua's
// "extra space", a pointer-sized slot Lua reserves for the host.
long long*& budgetSlot(lua_State* L) { return *static_cast<long long**>(lua_getextraspace(L)); }

void budgetHook(lua_State* L, lua_Debug*) {
    long long* budget = budgetSlot(L);
    *budget -= kHookInterval;
    if (*budget > 0) return;

    // Out of budget. A script could catch this error with pcall and carry on
    // looping, so from here the hook fires on every single instruction: each
    // pcall that swallows the error is hit again on its very next step, and
    // the error works its way out to the host. ScriptVM::call restores the
    // normal interval.
    lua_sethook(L, budgetHook, LUA_MASKCOUNT, 1);
    luaL_error(L, "script ran too long (infinite loop?)");
}

// print(...) -> engine log, arguments separated by tabs like stock Lua.
// Built with Lua's own buffer rather than std::string: luaL_tolstring can
// raise a Lua error (a failing __tostring), and Lua errors are longjmps that
// would skip a std::string's destructor.
int logPrint(lua_State* L) {
    int count = lua_gettop(L);
    luaL_Buffer line;
    luaL_buffinit(L, &line);
    for (int i = 1; i <= count; ++i) {
        if (i > 1) luaL_addchar(&line, '\t');
        luaL_tolstring(L, i, nullptr);
        luaL_addvalue(&line);
    }
    luaL_pushresult(&line);
    SDL_Log("[lua] %s", lua_tostring(L, -1));
    return 0;
}

void removeGlobal(lua_State* L, const char* name) {
    lua_pushnil(L);
    lua_setglobal(L, name);
}

void removeField(lua_State* L, const char* table, const char* field) {
    lua_getglobal(L, table);
    lua_pushnil(L);
    lua_setfield(L, -2, field);
    lua_pop(L, 1);
}

std::string popError(lua_State* L) {
    const char* message = lua_tostring(L, -1);
    std::string error = message ? message : "unknown error (non-string error value)";
    lua_pop(L, 1);
    return error;
}

}  // namespace

ScriptVM::ScriptVM() {
    state_ = luaL_newstate();
    budgetSlot(state_) = &budget_;

    // Only the libraries that can't reach outside the VM.
    luaL_requiref(state_, LUA_GNAME, luaopen_base, 1);
    luaL_requiref(state_, LUA_TABLIBNAME, luaopen_table, 1);
    luaL_requiref(state_, LUA_STRLIBNAME, luaopen_string, 1);
    luaL_requiref(state_, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(state_, 4);

    // The base library still has a few ways to load code from disk.
    removeGlobal(state_, "dofile");
    removeGlobal(state_, "loadfile");
    removeGlobal(state_, "load");

    removeField(state_, "math", "random");
    removeField(state_, "math", "randomseed");

    lua_pushcfunction(state_, logPrint);
    lua_setglobal(state_, "print");
}

ScriptVM::~ScriptVM() {
    if (state_) lua_close(state_);
}

bool ScriptVM::run(std::string_view source, const std::string& chunkName, std::string& error) {
    // A leading '@' tells Lua the name is a file name, so errors read
    // "enemies.lua:12: ..." rather than quoting the source text.
    std::string name = "@" + chunkName;
    if (luaL_loadbufferx(state_, source.data(), source.size(), name.c_str(), "t") != LUA_OK) {
        error = popError(state_);
        return false;
    }
    return call(0, 0, error);
}

bool ScriptVM::call(int argCount, int resultCount, std::string& error) {
    budget_ = kInstructionBudget;
    lua_sethook(state_, budgetHook, LUA_MASKCOUNT, kHookInterval);
    if (lua_pcall(state_, argCount, resultCount, 0) != LUA_OK) {
        error = popError(state_);
        return false;
    }
    return true;
}
