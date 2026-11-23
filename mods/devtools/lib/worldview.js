// The world and the player: teleports, the time, triggers (movers), characters and items that appear, health.
"use strict";

const Gold = [0.82, 0.66, 0.34, 1];
const Green = [0.45, 0.85, 0.45, 1];
const States = ["open", "opening", "closed", "closing"];

class WorldView {
    constructor(env) {
        this.env = env;          // { debug, player, npcs, world }
        this.hour = 12;
        this.minute = 0;
        this.pointQuery = "";
        this.lastPoint = null;
        this.points = [];
        this.point = null;
        this.trigQuery = "";
        this.triggers = [];
        this.trigger = null;
        this.trigTick = 0;
        this.spawnName = "";
        this.itemName = "";
        this.itemCount = "1";
        this.health = "100";
        this.message = "";
    }

    say(text) {
        this.message = text;
    }

    // --- time
    time(ui) {
        ui.withColor(Gold, () => ui.text("Time"));
        const t = this.env.world.time();
        if (t) ui.textDisabled(`day ${t.day}, ${String(t.hour).padStart(2, "0")}:${String(t.minute).padStart(2, "0")}`);
        this.hour = ui.sliderInt("hour", this.hour, 0, 23);
        this.minute = ui.sliderInt("minute", this.minute, 0, 59);
        if (ui.button("Set the time", -1)) {
            this.env.world.setTime(this.hour, this.minute);
            this.say(`the time is ${this.hour}:${String(this.minute).padStart(2, "0")}`);
        }
    }

    // --- the weather
    weather(ui) {
        ui.spacing();
        ui.separator();
        ui.withColor(Gold, () => ui.text("Weather"));
        ui.textDisabled(this.env.debug.rain() ? "it is raining" : "it is dry");
        if (ui.button("Rain on")) { this.env.debug.rain(1); this.say("the rain is on (the setting Rain of the options must not be OFF)"); }
        ui.sameLine();
        if (ui.button("Rain off")) { this.env.debug.rain(0); this.say("the rain is off"); }
        ui.sameLine();
        if (ui.button("Weather of the day")) { this.env.debug.rain(-1); this.say("the weather of the day"); }
    }

    // --- teleport
    teleport(ui) {
        ui.spacing();
        ui.separator();
        ui.withColor(Gold, () => ui.text("Teleport to a way point"));
        ui.setNextWidth(-ui.fontSize() * 4);
        this.pointQuery = ui.inputText("##pointquery", this.pointQuery);
        if (this.pointQuery !== this.lastPoint) {
            this.lastPoint = this.pointQuery;
            this.points = this.pointQuery.length >= 2 ? this.env.debug.points(this.pointQuery, 80) : [];
        }
        ui.sameLine();
        ui.textDisabled("2+ letters");
        ui.child("##points", -1, ui.fontSize() * 6, () => {
            ui.clipped(this.points.length, (i) => {
                if (ui.selectable(this.points[i] + "##p" + i, this.points[i] === this.point)) this.point = this.points[i];
            });
        });
        if (this.point && ui.button(`Go to ${this.point}`, -1)) {
            this.say(this.env.player.teleport(this.point) ? `at ${this.point}` : `cannot go to ${this.point}`);
        }
    }

    // --- triggers and movers
    triggerList(ui) {
        ui.spacing();
        ui.separator();
        ui.withColor(Gold, () => ui.text("Triggers and movers of the world"));
        ui.setNextWidth(-ui.fontSize() * 4);
        this.trigQuery = ui.inputText("##trigquery", this.trigQuery);
        // the states of the movers change, the list is read again from time to time
        if (this.trigQuery.length >= 2 && (++this.trigTick % 30 === 1 || this.trigQuery !== this.lastTrig)) {
            this.lastTrig = this.trigQuery;
            this.triggers = this.env.debug.triggers(this.trigQuery, 80);
        } else if (this.trigQuery.length < 2) {
            this.triggers = [];
        }
        ui.sameLine();
        ui.textDisabled("2+ letters");
        ui.child("##triggers", -1, ui.fontSize() * 6, () => {
            ui.clipped(this.triggers.length, (i) => {
                const [name, kind, state] = this.triggers[i];
                const label = kind === "mover" ? `${name}  [mover ${States[state] ?? state}]` : name;
                if (ui.selectable(label + "##t" + i, name === this.trigger)) this.trigger = name;
            });
        });
        if (this.trigger) {
            if (ui.button("Trigger")) this.say(this.env.debug.trigger(this.trigger, false) ? `triggered ${this.trigger}` : "no world");
            ui.sameLine();
            if (ui.button("Untrigger")) this.say(this.env.debug.trigger(this.trigger, true) ? `untriggered ${this.trigger}` : "no world");
        }
    }

    // --- characters and items
    make(ui) {
        ui.spacing();
        ui.separator();
        ui.withColor(Gold, () => ui.text("Characters and items"));
        const p = this.env.player;

        ui.setNextWidth(ui.fontSize() * 14);
        this.spawnName = ui.inputText("instance of a character##spawn", this.spawnName);
        ui.sameLine();
        if (ui.button("Spawn") && this.spawnName.trim() !== "") {
            // in front of the player
            const a = (p.rotation * Math.PI) / 180;
            const npc = this.env.npcs.spawn(this.spawnName.trim().toUpperCase(), p.x + Math.cos(a) * 250, p.y, p.z + Math.sin(a) * 250, p.rotation + 180);
            this.say(npc ? `${npc.name} is here` : `there is no instance ${this.spawnName}`);
        }

        ui.setNextWidth(ui.fontSize() * 14);
        this.itemName = ui.inputText("instance of an item##item", this.itemName);
        ui.sameLine();
        ui.setNextWidth(ui.fontSize() * 4);
        this.itemCount = ui.inputText("count##count", this.itemCount, "decimal");
        ui.sameLine();
        if (ui.button("Give") && this.itemName.trim() !== "")
            this.say(p.addItem(this.itemName.trim().toUpperCase(), Number(this.itemCount) || 1) ? `${this.itemCount} x ${this.itemName}` : `there is no instance ${this.itemName}`);
    }

    // --- the player
    player(ui) {
        ui.spacing();
        ui.separator();
        ui.withColor(Gold, () => ui.text("Player"));
        const p = this.env.player;
        if (!p.valid) {
            ui.textDisabled("there is no game");
            return;
        }
        ui.text(`${p.name}: ${p.health}/${p.maxHealth} HP, level ${p.level}`);
        ui.textDisabled(`at ${Math.round(p.x)} ${Math.round(p.y)} ${Math.round(p.z)}, ${Math.round(p.rotation)} deg, in ${this.env.world.name()}`);

        if (ui.button("Heal")) { p.setHealth(p.maxHealth); this.say("healed"); }
        ui.sameLine();
        const god = ui.checkbox("god mode", this.env.debug.god());
        if (god !== this.env.debug.god()) this.env.debug.god(god);
        ui.sameLine();
        const marvin = ui.checkbox("keys of the developer", this.env.debug.marvin());
        if (marvin !== this.env.debug.marvin()) this.env.debug.marvin(marvin);

        ui.setNextWidth(ui.fontSize() * 6);
        this.health = ui.inputText("health##hp", this.health, "decimal");
        ui.sameLine();
        if (ui.button("Set health")) p.setHealth(Number(this.health) || 0);
    }

    draw(ui) {
        this.time(ui);
        this.weather(ui);
        this.teleport(ui);
        this.triggerList(ui);
        this.make(ui);
        this.player(ui);
        if (this.message) {
            ui.spacing();
            ui.withColor(Green, () => ui.textWrapped(this.message));
        }
    }
}

module.exports = { WorldView };
