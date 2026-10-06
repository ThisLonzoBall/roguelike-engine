#include "game/scripts.h"

#include <SDL3/SDL.h>
#include <lua.hpp>

#include <cmath>
#include <optional>
#include <string_view>

#include "engine/file.h"
#include "engine/script.h"

// Note for everything in this file that Lua calls: Lua reports errors with a
// longjmp, which skips C++ destructors. So no object that owns memory
// (std::string, std::vector, ...) may be alive at a point where luaL_error
// can be raised; those functions validate into plain values first.

struct ScriptsImpl {
    std::unique_ptr<ScriptVM> vm;
    std::vector<EnemyType> types;
    std::vector<int> thinkRefs;      // parallel to `types`; registry ref or LUA_NOREF
    std::vector<BoonDef> boons;
    std::vector<int> applyRefs;      // parallel to `boons`; registry ref
    int instancesRef = LUA_NOREF;    // registry ref: table of enemy id -> self table
    int contextRef = LUA_NOREF;      // registry ref: the ctx table, reused every call
    Rng* activeRng = nullptr;        // set only while a think function is running
};

namespace {

constexpr float kMaxThinkSpeed = 2000.0f;  // px/s; clamps whatever a script returns

// C functions exposed to Lua find the ScriptsImpl they belong to through
// their first upvalue.
ScriptsImpl& implOf(lua_State* L) {
    return *static_cast<ScriptsImpl*>(lua_touserdata(L, lua_upvalueindex(1)));
}

// --- Reading fields out of a definition table --------------------------------

// Number at t[field], or `fallback` if absent. Raises a Lua error (which
// carries the script's file and line) if it's the wrong type or out of range.
double numberField(lua_State* L, int table, const char* field, double fallback, double min,
                   double max) {
    lua_getfield(L, table, field);
    double value = fallback;
    if (!lua_isnil(L, -1)) {
        if (lua_type(L, -1) != LUA_TNUMBER) luaL_error(L, "'%s' must be a number", field);
        value = lua_tonumber(L, -1);
        if (!(value >= min && value <= max)) {
            luaL_error(L, "'%s' must be between %f and %f", field, min, max);
        }
    }
    lua_pop(L, 1);
    return value;
}

// Reads {r, g, b} from the table at `index`. Returns false if it isn't three
// numbers. Components are clamped to 0-255.
bool readColor(lua_State* L, int index, Color& out) {
    if (!lua_istable(L, index)) return false;
    index = lua_absindex(L, index);

    uint8_t rgb[3];
    for (int i = 0; i < 3; ++i) {
        lua_rawgeti(L, index, i + 1);
        bool isNumber = lua_type(L, -1) == LUA_TNUMBER;
        double value = isNumber ? lua_tonumber(L, -1) : 0.0;
        lua_pop(L, 1);
        if (!isNumber || !std::isfinite(value)) return false;
        rgb[i] = static_cast<uint8_t>(value < 0.0 ? 0.0 : value > 255.0 ? 255.0 : value);
    }
    out = {rgb[0], rgb[1], rgb[2], 255};
    return true;
}

// Catches typos like `sped = 200`, which would otherwise be silently ignored.
void rejectUnknownFields(lua_State* L, int table, std::initializer_list<std::string_view> known) {
    lua_pushnil(L);
    while (lua_next(L, table) != 0) {
        lua_pop(L, 1);  // drop the value, keep the key for the next lua_next
        if (lua_type(L, -1) != LUA_TSTRING) luaL_error(L, "definition fields must be named");

        std::string_view key = lua_tostring(L, -1);
        bool found = false;
        for (std::string_view name : known) found = found || name == key;
        if (!found) luaL_error(L, "unknown field '%s'", lua_tostring(L, -1));
    }
}

// --- Functions exposed to Lua ------------------------------------------------

const EnemyType kEnemyDefaults;  // what omitted fields fall back to

// Enemy { ... } registers an enemy type.
int defineEnemy(lua_State* L) {
    ScriptsImpl& impl = implOf(L);
    luaL_checktype(L, 1, LUA_TTABLE);
    rejectUnknownFields(L, 1, {"name", "hp", "speed", "radius", "color", "contact_damage",
                               "min_depth", "weight", "think"});

    // Validate everything into plain values first (see the note at the top).
    // The name stays on the Lua stack, which keeps the pointer valid.
    lua_getfield(L, 1, "name");
    if (lua_type(L, -1) != LUA_TSTRING || lua_rawlen(L, -1) == 0) {
        luaL_error(L, "Enemy needs a 'name' (a non-empty string)");
    }
    const char* name = lua_tostring(L, -1);
    for (const EnemyType& existing : impl.types) {
        if (existing.name == name) luaL_error(L, "an enemy named '%s' is already defined", name);
    }

    const EnemyType& defaults = kEnemyDefaults;
    double hp = numberField(L, 1, "hp", defaults.hp, 1, 1000);
    double speed = numberField(L, 1, "speed", defaults.speed, 0, 1000);
    double radius = numberField(L, 1, "radius", defaults.radius, 4, 60);
    double contactDamage = numberField(L, 1, "contact_damage", defaults.contactDamage, 0, 100);
    double minDepth = numberField(L, 1, "min_depth", defaults.minDepth, 1, 1000);
    double weight = numberField(L, 1, "weight", defaults.weight, 1, 1000);

    Color color = defaults.color;
    lua_getfield(L, 1, "color");
    if (!lua_isnil(L, -1) && !readColor(L, -1, color)) {
        luaL_error(L, "'color' must be {r, g, b} with numbers from 0 to 255");
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "think");
    bool hasThink = lua_isfunction(L, -1);
    if (!hasThink && !lua_isnil(L, -1)) luaL_error(L, "'think' must be a function");

    // Nothing below can raise a Lua error.
    int thinkRef = LUA_NOREF;
    if (hasThink) {
        thinkRef = luaL_ref(L, LUA_REGISTRYINDEX);  // pops the function
    } else {
        lua_pop(L, 1);
    }

    EnemyType type;
    type.name = name;
    type.hp = static_cast<int>(hp);
    type.speed = static_cast<float>(speed);
    type.radius = static_cast<float>(radius);
    type.contactDamage = static_cast<int>(contactDamage);
    type.minDepth = static_cast<int>(minDepth);
    type.weight = static_cast<int>(weight);
    type.color = color;
    type.hasThink = hasThink;

    impl.types.push_back(std::move(type));
    impl.thinkRefs.push_back(thinkRef);
    return 0;
}

// Boon { ... } registers a boon.
int defineBoon(lua_State* L) {
    ScriptsImpl& impl = implOf(L);
    luaL_checktype(L, 1, LUA_TTABLE);
    rejectUnknownFields(L, 1, {"name", "desc", "max_stacks", "apply"});

    // As in defineEnemy: plain values first, strings kept alive on the Lua stack.
    lua_getfield(L, 1, "name");
    if (lua_type(L, -1) != LUA_TSTRING || lua_rawlen(L, -1) == 0) {
        luaL_error(L, "Boon needs a 'name' (a non-empty string)");
    }
    const char* name = lua_tostring(L, -1);
    for (const BoonDef& existing : impl.boons) {
        if (existing.name == name) luaL_error(L, "a boon named '%s' is already defined", name);
    }

    lua_getfield(L, 1, "desc");
    if (!lua_isnil(L, -1) && lua_type(L, -1) != LUA_TSTRING) {
        luaL_error(L, "'desc' must be a string");
    }
    const char* desc = lua_isnil(L, -1) ? "" : lua_tostring(L, -1);

    double maxStacks = numberField(L, 1, "max_stacks", 1, 1, 99);

    lua_getfield(L, 1, "apply");
    if (!lua_isfunction(L, -1)) luaL_error(L, "Boon needs an 'apply' function");

    // Nothing below can raise a Lua error.
    int applyRef = luaL_ref(L, LUA_REGISTRYINDEX);  // pops the function

    BoonDef boon;
    boon.name = name;
    boon.desc = desc;
    boon.maxStacks = static_cast<int>(maxStacks);

    impl.boons.push_back(std::move(boon));
    impl.applyRefs.push_back(applyRef);
    return 0;
}

// --- PlayerStats <-> Lua table -----------------------------------------------

// One row per stat: its script name, where it lives, and the range it's
// clamped to after boons have been applied.
struct StatField {
    const char* name;
    int PlayerStats::* intMember;
    float PlayerStats::* floatMember;
    double min;
    double max;
};

constexpr StatField kStatFields[] = {
    {"max_hp", &PlayerStats::maxHp, nullptr, 1, 20},
    {"move_speed", nullptr, &PlayerStats::moveSpeed, 50, 800},
    {"dash_cooldown", &PlayerStats::dashCooldownTicks, nullptr, 0, 600},
    {"attack_damage", &PlayerStats::attackDamage, nullptr, 1, 99},
    {"attack_radius", nullptr, &PlayerStats::attackRadius, 8, 150},
    // At least 1: the swing's state ends on its last tick, so with no recovery
    // the final active tick would be lost.
    {"attack_recovery", &PlayerStats::attackRecoveryTicks, nullptr, 1, 120},
    {"knockback", nullptr, &PlayerStats::attackKnockback, 0, 3000},
    {"mercy_ticks", &PlayerStats::hitInvulnTicks, nullptr, 0, 600},
    {"heal_on_clear", &PlayerStats::healOnClear, nullptr, 0, 20},
};

void pushStats(lua_State* L, const PlayerStats& stats) {
    lua_newtable(L);
    for (const StatField& field : kStatFields) {
        double value = field.intMember ? stats.*field.intMember : stats.*field.floatMember;
        lua_pushnumber(L, value);
        lua_setfield(L, -2, field.name);
    }
}

// Reads the stats table at `index` into `out`. Returns the name of the first
// field that isn't a finite number, or nullptr if all are fine.
const char* readStats(lua_State* L, int index, PlayerStats& out) {
    index = lua_absindex(L, index);
    for (const StatField& field : kStatFields) {
        lua_getfield(L, index, field.name);
        bool isNumber = lua_type(L, -1) == LUA_TNUMBER;
        double value = lua_tonumber(L, -1);
        lua_pop(L, 1);
        if (!isNumber || !std::isfinite(value)) return field.name;

        value = value < field.min ? field.min : value > field.max ? field.max : value;
        if (field.intMember) {
            out.*field.intMember = static_cast<int>(std::floor(value + 0.5));
        } else {
            out.*field.floatMember = static_cast<float>(value);
        }
    }
    return nullptr;
}

// rand() -> number in [0, 1) from the world's seeded generator.
int scriptRand(lua_State* L) {
    ScriptsImpl& impl = implOf(L);
    if (!impl.activeRng) luaL_error(L, "rand() can only be called from inside think");
    lua_pushnumber(L, impl.activeRng->unit());
    return 1;
}

void setNumber(lua_State* L, int table, const char* field, double value) {
    lua_pushnumber(L, value);
    lua_setfield(L, table, field);
}

EnemyType builtinChaser() {
    EnemyType type;
    type.name = "chaser";
    return type;
}

}  // namespace

Scripts::Scripts() : impl_(std::make_unique<ScriptsImpl>()) {
    impl_->types.push_back(builtinChaser());
    impl_->thinkRefs.push_back(LUA_NOREF);
}

Scripts::~Scripts() = default;
Scripts::Scripts(Scripts&&) noexcept = default;
Scripts& Scripts::operator=(Scripts&&) noexcept = default;

void Scripts::load(const std::string& scriptDir) {
    ScriptsImpl& impl = *impl_;

    // A brand new VM, so nothing from the previous load survives.
    impl.types.clear();
    impl.thinkRefs.clear();
    impl.boons.clear();
    impl.applyRefs.clear();
    impl.vm = std::make_unique<ScriptVM>();
    lua_State* L = impl.vm->state();

    lua_pushlightuserdata(L, &impl);
    lua_pushcclosure(L, defineEnemy, 1);
    lua_setglobal(L, "Enemy");

    lua_pushlightuserdata(L, &impl);
    lua_pushcclosure(L, defineBoon, 1);
    lua_setglobal(L, "Boon");

    lua_pushlightuserdata(L, &impl);
    lua_pushcclosure(L, scriptRand, 1);
    lua_setglobal(L, "rand");

    lua_newtable(L);
    impl.instancesRef = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_newtable(L);
    impl.contextRef = luaL_ref(L, LUA_REGISTRYINDEX);

    for (const std::string& path : listFiles(scriptDir, ".lua")) {
        std::string fileName = path.substr(path.find_last_of('/') + 1);
        std::optional<std::string> source = readTextFile(path);
        if (!source) {
            SDL_Log("Script %s: could not be read", path.c_str());
            continue;
        }

        std::string error;
        if (!impl.vm->run(*source, fileName, error)) {
            SDL_Log("Script error: %s", error.c_str());
        }
    }

    if (impl.types.empty()) {
        SDL_Log("No enemy types defined by scripts in '%s'; using the built-in chaser",
                scriptDir.c_str());
        impl.types.push_back(builtinChaser());
        impl.thinkRefs.push_back(LUA_NOREF);
    } else {
        SDL_Log("Loaded %d enemy type(s) from '%s'", static_cast<int>(impl.types.size()),
                scriptDir.c_str());
    }
    SDL_Log("Loaded %d boon(s)", static_cast<int>(impl.boons.size()));
}

const std::vector<EnemyType>& Scripts::enemyTypes() const { return impl_->types; }

int Scripts::pickEnemyType(int depth, Rng& rng) const {
    const std::vector<EnemyType>& types = impl_->types;

    // If nothing is allowed this early in the run, allow everything rather
    // than spawn nothing.
    uint32_t total = 0;
    for (const EnemyType& type : types) {
        if (type.minDepth <= depth) total += static_cast<uint32_t>(type.weight);
    }
    bool ignoreDepth = total == 0;
    if (ignoreDepth) {
        for (const EnemyType& type : types) total += static_cast<uint32_t>(type.weight);
    }

    uint32_t roll = rng.next() % total;
    for (size_t i = 0; i < types.size(); ++i) {
        if (!ignoreDepth && types[i].minDepth > depth) continue;
        uint32_t weight = static_cast<uint32_t>(types[i].weight);
        if (roll < weight) return static_cast<int>(i);
        roll -= weight;
    }
    return 0;  // unreachable: the weights sum to `total`
}

ThinkResult Scripts::think(const ThinkInput& input, Rng& rng) {
    ScriptsImpl& impl = *impl_;
    EnemyType& type = impl.types[static_cast<size_t>(input.type)];
    ThinkResult result;

    Vec2 toPlayer = input.playerPos - input.pos;
    float dist = length(toPlayer);
    Vec2 dir = normalize(toPlayer);
    float reach = input.radius + input.playerRadius;

    auto builtinChase = [&] {
        if (!input.stunned && dist > reach) result.velocity = dir * type.speed;
    };

    if (!type.hasThink || type.thinkBroken) {
        builtinChase();
        return result;
    }

    lua_State* L = impl.vm->state();
    int top = lua_gettop(L);

    // self = instances[id], created on first use.
    lua_rawgeti(L, LUA_REGISTRYINDEX, impl.instancesRef);
    lua_rawgeti(L, -1, static_cast<lua_Integer>(input.id));
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_rawseti(L, -3, static_cast<lua_Integer>(input.id));
    }
    lua_remove(L, -2);  // drop `instances`
    int self = lua_gettop(L);

    setNumber(L, self, "x", input.pos.x);
    setNumber(L, self, "y", input.pos.y);
    setNumber(L, self, "speed", type.speed);
    setNumber(L, self, "radius", input.radius);
    lua_pushinteger(L, input.hp);
    lua_setfield(L, self, "hp");
    lua_pushinteger(L, input.ageTicks);
    lua_setfield(L, self, "age");
    lua_pushboolean(L, input.stunned);
    lua_setfield(L, self, "stunned");

    // Stack for the call: think, self, ctx. `self` stays underneath so its
    // tint can be read afterwards.
    lua_rawgeti(L, LUA_REGISTRYINDEX, impl.thinkRefs[static_cast<size_t>(input.type)]);
    lua_pushvalue(L, self);
    lua_rawgeti(L, LUA_REGISTRYINDEX, impl.contextRef);
    int ctx = lua_gettop(L);
    setNumber(L, ctx, "player_x", input.playerPos.x);
    setNumber(L, ctx, "player_y", input.playerPos.y);
    setNumber(L, ctx, "dist", dist);
    setNumber(L, ctx, "dir_x", dir.x);
    setNumber(L, ctx, "dir_y", dir.y);
    setNumber(L, ctx, "reach", reach);
    setNumber(L, ctx, "dt", input.dt);
    lua_pushinteger(L, input.depth);
    lua_setfield(L, ctx, "depth");

    std::string error;
    impl.activeRng = &rng;
    bool ok = impl.vm->call(2, 2, error);
    impl.activeRng = nullptr;

    // think returns vx, vy; each may be nil, meaning zero.
    double vx = 0.0, vy = 0.0;
    if (ok) {
        int typeX = lua_type(L, -2), typeY = lua_type(L, -1);
        bool validTypes = (typeX == LUA_TNUMBER || typeX == LUA_TNIL) &&
                          (typeY == LUA_TNUMBER || typeY == LUA_TNIL);
        vx = lua_tonumber(L, -2);
        vy = lua_tonumber(L, -1);
        if (!validTypes || !std::isfinite(vx) || !std::isfinite(vy)) {
            ok = false;
            error = "think must return two finite numbers (vx, vy) or nothing";
        }
    }

    if (!ok) {
        SDL_Log("Script error in enemy '%s': %s. Using the built-in chase until scripts are "
                "reloaded.",
                type.name.c_str(), error.c_str());
        type.thinkBroken = true;
        lua_settop(L, top);
        builtinChase();
        return result;
    }

    if (!input.stunned) {
        result.velocity = {static_cast<float>(vx), static_cast<float>(vy)};
        float speed = length(result.velocity);
        if (speed > kMaxThinkSpeed) result.velocity = result.velocity * (kMaxThinkSpeed / speed);
    }

    lua_getfield(L, self, "tint");
    result.hasTint = readColor(L, -1, result.tint);

    lua_settop(L, top);
    return result;
}

const std::vector<BoonDef>& Scripts::boons() const { return impl_->boons; }

int Scripts::findBoon(std::string_view name) const {
    for (size_t i = 0; i < impl_->boons.size(); ++i) {
        if (impl_->boons[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

PlayerStats Scripts::computeStats(const PlayerStats& base,
                                  const std::vector<std::string>& ownedBoons) {
    ScriptsImpl& impl = *impl_;
    PlayerStats stats = base;
    if (!impl.vm) return stats;
    lua_State* L = impl.vm->state();

    for (const std::string& name : ownedBoons) {
        int index = findBoon(name);
        if (index < 0) continue;
        BoonDef& boon = impl.boons[static_cast<size_t>(index)];
        if (boon.applyBroken) continue;

        // Each boon gets a fresh table holding the stats so far, so one that
        // fails can't leave them half-changed. Stack: table, apply, table.
        int top = lua_gettop(L);
        pushStats(L, stats);
        lua_rawgeti(L, LUA_REGISTRYINDEX, impl.applyRefs[static_cast<size_t>(index)]);
        lua_pushvalue(L, -2);

        std::string error;
        bool ok = impl.vm->call(1, 0, error);
        if (ok) {
            PlayerStats updated = stats;
            if (const char* badField = readStats(L, -1, updated)) {
                ok = false;
                error = std::string("apply left stats.") + badField + " as something other than a number";
            } else {
                stats = updated;
            }
        }
        if (!ok) {
            SDL_Log("Script error in boon '%s': %s. Ignoring this boon until scripts are reloaded.",
                    boon.name.c_str(), error.c_str());
            boon.applyBroken = true;
        }
        lua_settop(L, top);
    }
    return stats;
}

void Scripts::forgetEnemy(uint32_t id) {
    if (!impl_->vm) return;
    lua_State* L = impl_->vm->state();
    lua_rawgeti(L, LUA_REGISTRYINDEX, impl_->instancesRef);
    lua_pushnil(L);
    lua_rawseti(L, -2, static_cast<lua_Integer>(id));
    lua_pop(L, 1);
}

void Scripts::forgetAllEnemies() {
    if (!impl_->vm) return;
    lua_State* L = impl_->vm->state();
    luaL_unref(L, LUA_REGISTRYINDEX, impl_->instancesRef);
    lua_newtable(L);
    impl_->instancesRef = luaL_ref(L, LUA_REGISTRYINDEX);
}
