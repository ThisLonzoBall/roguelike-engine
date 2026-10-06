#pragma once

#include <string>
#include <string_view>

struct lua_State;

// A sandboxed Lua virtual machine. Knows nothing about the game: it only
// creates the VM, runs chunks, and calls functions safely. Game code registers
// its own functions on state() and decides what scripts are for.
//
// Sandbox:
//  - only the base, table, string and math libraries are available; there is
//    no file, OS or module access
//  - math.random is removed so scripts can't introduce non-determinism; the
//    game supplies its own seeded source
//  - every call runs under an instruction budget, so an infinite loop in a
//    script becomes an error instead of a frozen game
//  - `print` goes to the engine log
class ScriptVM {
public:
    ScriptVM();
    ~ScriptVM();
    ScriptVM(const ScriptVM&) = delete;
    ScriptVM& operator=(const ScriptVM&) = delete;

    lua_State* state() { return state_; }

    // Compiles and runs `source`. `chunkName` is what error messages call it
    // (normally the file name). Returns false and fills `error` on a syntax or
    // runtime error.
    bool run(std::string_view source, const std::string& chunkName, std::string& error);

    // Calls a function. Expects the Lua stack to hold the function followed by
    // `argCount` arguments, as lua_pcall does. On success the `resultCount`
    // results are left on the stack; on failure the stack is left as it was
    // below the function, `error` is filled, and false is returned.
    bool call(int argCount, int resultCount, std::string& error);

private:
    lua_State* state_ = nullptr;
    long long budget_ = 0;  // instructions left in the current call
};
