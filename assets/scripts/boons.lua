-- Boons: upgrades offered three at a time after a room is cleared. The player
-- picks one and keeps it until the run ends.
--
--   name        unique name, shown on the choice screen (12 characters or
--               fewer fits the card title at full size)
--   desc        one or two short sentences
--   max_stacks  how many times it can be taken (default 1)
--   apply       function(stats), run once per stack whenever the player's
--               stats are recomputed. Change the fields you care about.
--
-- stats fields (base value):
--   max_hp           5     maximum health
--   move_speed       300   px/s
--   dash_cooldown    18    ticks between dashes (60 ticks = 1 second)
--   attack_damage    1     damage per hit
--   attack_radius    30    px; size of the swing and how far it reaches
--   attack_recovery  9     ticks after a swing before you can act again
--   knockback        520   px/s shove given to enemies you hit
--   mercy_ticks      45    ticks of invulnerability after being hit
--   heal_on_clear    1     health restored when a room is cleared
--
-- The engine clamps every stat to a sane range afterwards and rounds the
-- whole-number ones, so apply functions don't need to guard against extremes.

Boon {
    name = "Heavy Blow",
    desc = "Attacks deal +1 damage.",
    max_stacks = 2,
    apply = function(stats)
        stats.attack_damage = stats.attack_damage + 1
    end,
}

Boon {
    name = "Swift Strike",
    desc = "Recover from attacks 35% faster.",
    max_stacks = 2,
    apply = function(stats)
        stats.attack_recovery = stats.attack_recovery * 0.65
    end,
}

Boon {
    name = "Long Reach",
    desc = "Attacks are 25% larger and reach further.",
    max_stacks = 2,
    apply = function(stats)
        stats.attack_radius = stats.attack_radius * 1.25
    end,
}

Boon {
    name = "Bruiser",
    desc = "Hits knock enemies back 60% harder.",
    apply = function(stats)
        stats.knockback = stats.knockback * 1.6
    end,
}

Boon {
    name = "Fleet Foot",
    desc = "Move 15% faster.",
    max_stacks = 2,
    apply = function(stats)
        stats.move_speed = stats.move_speed * 1.15
    end,
}

Boon {
    name = "Quick Step",
    desc = "Dash recharges 35% faster.",
    max_stacks = 2,
    apply = function(stats)
        stats.dash_cooldown = stats.dash_cooldown * 0.65
    end,
}

Boon {
    name = "Thick Skin",
    desc = "+1 maximum health, and heal 1 now.",
    max_stacks = 3,
    apply = function(stats)
        stats.max_hp = stats.max_hp + 1
    end,
}

Boon {
    name = "Second Wind",
    desc = "Heal 1 more health each time you clear a room.",
    apply = function(stats)
        stats.heal_on_clear = stats.heal_on_clear + 1
    end,
}

Boon {
    name = "Ghost Step",
    desc = "Stay invulnerable 60% longer after being hit.",
    apply = function(stats)
        stats.mercy_ticks = stats.mercy_ticks * 1.6
    end,
}
