class_name Enemy
extends RefCounted
## Base of every foe: spawning, stun, hit flash, knockback, contact damage, death.

var id: int = 0
var kind: String = ""
## Model id for the renderer.
var look: String = ""
var x: float
var y: float
var px: float
var py: float
var vx := 0.0
var vy := 0.0
var r := 0.6
var hp := 5.0
var max_hp := 5.0
var color: int = 0xff2fb9
var value := 100.0
var notes := 2
var heavy := false
var elite := false
var boss := false
var t := 0.0
var st := 0.0
var state := "idle"
var spawn_t := 45
var dead := false
var stun := 0.0
var flash := 0.0
var hurtable := true
var targetable := true
var contact := true
## Facing angle and a free animation channel (render).
var rot := 0.0
var spin := 0.0
## Visibility (phantoms fade).
var vis := 1.0
## Damage taken is multiplied by this.
var armor := 1.0
var shielded := 0

func _init(px_: float, py_: float) -> void:
	x = px_; px = px_
	y = py_; py = py_

func is_active() -> bool:
	return not dead and spawn_t <= 0

## Tougher, bigger, crowned; may drop a heal.
func make_elite() -> Enemy:
	elite = true
	max_hp *= 2.6
	hp = max_hp
	r *= 1.25
	value *= 3.0
	notes *= 3
	return self

func to(s: String) -> void:
	state = s
	st = 0.0

func angle_to(w: World) -> float:
	return atan2(w.player.y - y, w.player.x - x)

func dist_to(w: World) -> float:
	return U.hypot(w.player.x - x, w.player.y - y)

func tick(w: World) -> void:
	px = x
	py = y
	if dead:
		return
	if spawn_t > 0:
		spawn_t -= 1
		return
	var k := w.k
	t += k
	st += k
	if flash > 0.0:
		flash = maxf(0.0, flash - 0.12)
	if shielded > 0:
		shielded -= 1
	if stun > 0.0:
		stun -= k
		vx *= 0.86
		vy *= 0.86
	else:
		update(w, k)
	x += (vx * k) / 60.0
	y += (vy * k) / 60.0
	var c := w.arena.confine(x, y, r)
	x = c.x
	y = c.y
	if contact and vis > 0.5:
		var p := w.player
		if U.hypot(p.x - x, p.y - y) < r + 0.3:
			w.hurt_player(x, y)

func update(_w: World, _k: float) -> void:
	pass

## Returns false when the hit was blocked.
func hit(w: World, d: float, from_x: float, from_y: float, k: int = -1) -> bool:
	if not hurtable or dead or spawn_t > 0:
		return false
	if blocks(w, from_x, from_y, k):
		w.emit({"t": "hit", "x": x + (from_x - x) * 0.5, "y": y + (from_y - y) * 0.5, "color": 0xffffff, "big": false})
		w.sfx("block", 1.0 + w.fx_rand() * 0.2, 0.5)
		return false
	var dd := d * (0.25 if shielded > 0 else armor)
	hp -= dd
	flash = 1.0
	if not heavy:
		var a := atan2(y - from_y, x - from_x)
		vx += cos(a) * minf(8.0, dd * 2.2)
		vy += sin(a) * minf(8.0, dd * 2.2)
	w.emit({"t": "hit", "x": x, "y": y, "color": color, "big": dd >= 3.0})
	if hp <= 0.0:
		die(w)
	return true

## Shields and mirrors override this.
func blocks(_w: World, _fx: float, _fy: float, _k: int) -> bool:
	return false

func die(w: World) -> void:
	if dead:
		return
	dead = true
	w.killed(self)
	if elite and w.rng.chance(0.4):
		w.drop(x, y, 1, "heal")
	on_death(w)

func on_death(_w: World) -> void:
	pass

## Breaks references to other objects when the foe leaves the world (no reference cycles left behind).
func release() -> void:
	pass

## Steering towards a point with a maximum speed.
func steer(tx: float, ty: float, speed: float, accel: float = 0.08) -> void:
	var dx := tx - x
	var dy := ty - y
	var d := U.hypot(dx, dy)
	if d == 0.0:
		d = 1.0
	vx += ((dx / d) * speed - vx) * accel
	vy += ((dy / d) * speed - vy) * accel
