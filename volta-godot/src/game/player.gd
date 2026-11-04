class_name Player
extends RefCounted
## Volta: movement, dash (charges, i-frames, beat refund), Pulse (reflect), Overdrive, weapon.

const SPEED := 11.5
const ACCEL := 22.0
const RADIUS := 0.32
const GRAZE := 1.15
const DASH_FRAMES := 9
const DASH_DIST := 5.4
const DASH_RECHARGE := 50.0
const PULSE_FRAMES := 8
const PULSE_RADIUS := 3.1
const PULSE_COOLDOWN := 30
const PULSE_MISS_COOLDOWN := 48
const HURT_INV := 80

var x := 0.0
var y := 0.0
var px := 0.0
var py := 0.0
var vx := 0.0
var vy := 0.0
var aim := 0.0
var hp := 4
var max_hp := 4
var inv := 0
var dead := false
var dead_t := 0
var dash_charges := 2
var max_dash := 2
var dash_refill := 0.0
var dash_t := 0
var dash_a := 0.0
var dash_judge := "off"
var _dash_hit: Dictionary = {}
var pulse_t := 0
var pulse_cd := 0
var pulse_r := PULSE_RADIUS
var pulse_judge := "off"
var pulse_hits := 0
var weapon := "prism"
var weapon_level := 1
var charge := 0
var charge_start_beat := -1.0
var last_perfect_beat := -9
var firing := false
## Overdrive end, in beats (0 = off).
var od_end := 0.0
var t := 0

func is_overdrive() -> bool:
	return od_end > 0.0

func is_dashing() -> bool:
	return dash_t > 0

func invulnerable() -> bool:
	return inv > 0 or dash_t > 0 or dead

func update(w: World, c: Controls) -> void:
	t += 1
	px = x
	py = y
	if dead:
		dead_t += 1
		vx *= 0.9
		vy *= 0.9
		return
	if inv > 0:
		inv -= 1
	var mods := w.mods
	if c.ax != 0.0 or c.ay != 0.0:
		aim = atan2(c.ay, c.ax)
	# Dash.
	if dash_charges < max_dash:
		dash_refill += 1.0
		if dash_refill >= DASH_RECHARGE * mods.dash_recharge:
			dash_refill = 0.0
			dash_charges += 1
	if c.dash and dash_t == 0 and dash_charges > 0:
		var ml := U.hypot(c.mx, c.my)
		dash_a = atan2(c.my, c.mx) if ml > 0.2 else aim
		dash_t = DASH_FRAMES
		dash_charges -= 1
		dash_judge = w.beat.judge()
		w.judged(dash_judge, "dash", x, y)
		if dash_judge != "off":
			dash_charges = mini(max_dash, dash_charges + 1)
		w.emit({"t": "dash", "x": x, "y": y, "a": dash_a, "judge": dash_judge})
		w.sfx("dash", 1.15 if dash_judge == "perfect" else 1.0)
	if dash_t > 0:
		var sp := (DASH_DIST * mods.dash_dist) / DASH_FRAMES
		vx = cos(dash_a) * sp * 60.0
		vy = sin(dash_a) * sp * 60.0
		dash_t -= 1
		if mods.has.has("sillage"):
			for e in w.enemies:
				if not e.is_active() or not e.hurtable or _dash_hit.has(e.id):
					continue
				if U.hypot(e.x - x, e.y - y) < e.r + 1.0:
					_dash_hit[e.id] = true
					e.hit(w, 4.0 * mods.damage, x, y)
		if dash_t == 0:
			_dash_hit.clear()
			vx *= 0.35
			vy *= 0.35
			if dash_judge == "perfect":
				w.dash_burst(x, y)
	else:
		var sp := SPEED * mods.speed * (0.6 if firing and weapon == "lance" and charge > 0 else 1.0)
		var k := U.damp(ACCEL)
		vx += (c.mx * sp - vx) * k
		vy += (c.my * sp - vy) * k
	x += vx / 60.0
	y += vy / 60.0
	var cf := w.arena.confine(x, y, RADIUS)
	x = cf.x
	y = cf.y
	# Pulse.
	if pulse_cd > 0:
		pulse_cd -= 1
	if c.pulse and w.can_pulse and pulse_cd == 0 and pulse_t == 0:
		pulse_judge = w.beat.judge()
		pulse_r = PULSE_RADIUS * mods.pulse_radius * (1.45 if pulse_judge == "perfect" else (1.2 if pulse_judge == "great" else 1.0))
		pulse_t = PULSE_FRAMES
		pulse_hits = 0
		w.judged(pulse_judge, "pulse", x, y)
		w.emit({"t": "pulse", "x": x, "y": y, "r": pulse_r, "judge": pulse_judge})
		w.sfx("pulse", 1.2 if pulse_judge == "perfect" else 1.0)
	if pulse_t > 0:
		pulse_hits += w.reflect(x, y, pulse_r, pulse_judge)
		pulse_t -= 1
		if pulse_t == 0:
			pulse_cd = PULSE_COOLDOWN if pulse_hits > 0 else PULSE_MISS_COOLDOWN
	# Overdrive.
	if od_end > 0.0 and w.beat.beat >= od_end:
		od_end = 0.0
		w.emit({"t": "overdrive", "on": false})
	if c.overdrive and w.can_overdrive and od_end == 0.0 and w.gauge >= 100.0:
		w.gauge = 0.0
		od_end = ceilf(w.beat.beat) + 16.0 * mods.overdrive
		w.emit({"t": "overdrive", "on": true})
		w.emit({"t": "flash", "color": 0xffffff, "a": 0.8})
		w.emit({"t": "shake", "a": 0.6})
		w.sfx("overdrive")
		w.clear_bullets(x, y, 9.0, true)
	Weapons.update(weapon, w, self, c)
	aim = fposmod(aim, U.TAU_F)
	hp = clampi(hp, 0, max_hp)
