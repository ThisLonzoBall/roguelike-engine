-- Enemy types. Each Enemy { ... } block registers one.
--
-- Stats (all optional except name):
--   name            unique name
--   hp              hits to kill                         (default 3)
--   speed           px/s, given to think as self.speed   (default 140)
--   radius          body size in px                      (default 14)
--   color           {r, g, b}, 0-255
--   contact_damage  damage dealt by touching the player  (default 1)
--   min_depth       first room of a run it can appear in (default 1)
--   weight          relative spawn chance                (default 1)
--
-- Behaviour (optional): think(self, ctx) runs once per simulation tick
-- (60 per second) and returns the velocity the enemy wants, vx, vy in px/s.
-- Returning nothing means stand still. Without a think function the enemy
-- just walks at the player.
--
--   self  x, y, hp, age (ticks alive), stunned, speed, radius are set by the
--         engine before every call. Any other field is yours and persists for
--         the enemy's lifetime. Set self.tint = {r, g, b} to override the
--         enemy's color; set it back to nil to restore it.
--   ctx   player_x, player_y, dist (to the player), dir_x, dir_y (unit vector
--         toward the player), reach (distance at which the bodies touch), dt,
--         depth.
--
-- rand() returns a random number in [0, 1). math.random is not available:
-- rand() draws from the room's seed, which keeps runs repeatable.
--
-- The engine handles collision, knockback and damage. While an enemy is
-- stunned from a hit, think still runs but its movement is ignored.

-- Walks straight at the player.
Enemy {
    name = "chaser",
    hp = 3,
    speed = 140,
    radius = 14,
    color = {120, 200, 90},
    weight = 4,

    think = function(self, ctx)
        if ctx.dist <= ctx.reach then return 0, 0 end
        return ctx.dir_x * self.speed, ctx.dir_y * self.speed
    end,
}

-- Small, fast and fragile. Weaves from side to side as it closes in, each one
-- out of step with the others.
Enemy {
    name = "swarmer",
    hp = 1,
    speed = 215,
    radius = 9,
    color = {235, 200, 80},
    weight = 3,

    think = function(self, ctx)
        self.phase = self.phase or rand() * 6.28

        -- Add a sideways push (perpendicular to the direction to the player)
        -- that swings back and forth over time.
        local sway = math.sin(self.age * 0.22 + self.phase) * 0.9
        local vx = ctx.dir_x - ctx.dir_y * sway
        local vy = ctx.dir_y + ctx.dir_x * sway
        local len = math.sqrt(vx * vx + vy * vy)
        if len == 0 then return 0, 0 end
        return vx / len * self.speed, vy / len * self.speed
    end,
}

-- Lumbers into range, flashes as a warning, then lunges in a straight line.
-- The direction is locked when the warning ends, so a dash sideways dodges it.
local CHARGE_RANGE = 280
local WINDUP_TICKS = 32
local CHARGE_TICKS = 24
local CHARGE_SPEED = 640
local REST_TICKS = 45

Enemy {
    name = "charger",
    hp = 4,
    speed = 85,
    radius = 17,
    color = {200, 90, 160},
    contact_damage = 2,
    min_depth = 2,
    weight = 2,

    think = function(self, ctx)
        self.state = self.state or "approach"

        -- Getting hit cancels whatever it was doing.
        if self.stunned then
            self.state, self.timer, self.tint = "rest", REST_TICKS / 2, nil
            return 0, 0
        end

        if self.state == "approach" then
            if ctx.dist < CHARGE_RANGE then
                self.state, self.timer = "windup", WINDUP_TICKS
            end
            return ctx.dir_x * self.speed, ctx.dir_y * self.speed

        elseif self.state == "windup" then
            -- Blink faster as the lunge gets closer.
            local blink = self.timer > WINDUP_TICKS / 2 and 8 or 4
            self.tint = (self.timer // blink) % 2 == 0 and {255, 255, 255} or {255, 60, 60}
            self.timer = self.timer - 1
            if self.timer <= 0 then
                self.state, self.timer = "charge", CHARGE_TICKS
                self.charge_x, self.charge_y = ctx.dir_x, ctx.dir_y
                self.tint = {255, 60, 60}
            end
            return 0, 0

        elseif self.state == "charge" then
            self.timer = self.timer - 1
            if self.timer <= 0 then
                self.state, self.timer, self.tint = "rest", REST_TICKS, nil
            end
            return self.charge_x * CHARGE_SPEED, self.charge_y * CHARGE_SPEED

        else -- rest
            self.timer = self.timer - 1
            if self.timer <= 0 then self.state = "approach" end
            return 0, 0
        end
    end,
}
