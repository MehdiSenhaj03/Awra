class_name Bulwark
extends Enemy
## Rempart: shots from the front are blocked. Flank it, stun it with a Pulse, or crack the shield with accented
## shots (on the beat): six cracks and it shatters for two bars.

var shield_a := 0.0
var guard := 6
## Frames left with the shield broken.
var broken := 0

func _init(px_: float, py_: float) -> void:
	super(px_, py_)
	kind = "bulwark"; look = "bulwark"
	r = 1.05; hp = 26.0; max_hp = 26.0; value = 320.0; notes = 5; heavy = true
	color = P.GOLD

func update(w: World, _k: float) -> void:
	steer(w.player.x, w.player.y, 2.2, 0.03)
	shield_a += clampf(U.angle_diff(shield_a, angle_to(w)), -0.022, 0.022)
	rot = shield_a
	if broken > 0:
		broken -= 1
		if broken == 0:
			guard = 9 if elite else 6
			w.emit({"t": "ring", "x": x, "y": y, "r": 2.0, "color": P.GOLD, "life": 20})
	if w.beat.crossed(1) and w.beat.index(1) % 4 == 2 and t > 60.0:
		P.fan(w, x, y, shield_a, 11 if elite else 7, 1.3, 8.5, {"color": P.GOLD, "kind": EnemyBullets.BIG, "r": 0.42})
		w.sfx("enemyShot", 0.6, 0.6)

func blocks(w: World, fx: float, fy: float, k: int) -> bool:
	if stun > 0.0 or broken > 0:
		return false
	if absf(U.angle_diff(shield_a, atan2(fy - y, fx - x))) >= 1.05:
		return false
	if k == Shots.ACCENT or k == Shots.PERFECT or k == Shots.LANCE:
		guard -= 1 if k == Shots.ACCENT else 2
		w.emit({"t": "hit", "x": x + cos(shield_a) * r, "y": y + sin(shield_a) * r, "color": P.GOLD, "big": true})
		if guard <= 0:
			broken = int(U.jround(w.beat.spb * 60.0 * 8.0))
			flash = 1.0
			w.emit({"t": "boom", "x": x, "y": y, "r": 2.5, "color": P.GOLD})
			w.emit({"t": "judge", "x": x, "y": y, "judge": "perfect", "what": "break"})
			w.sfx("bigBoom", 1.4, 0.6)
			return false
	return true
