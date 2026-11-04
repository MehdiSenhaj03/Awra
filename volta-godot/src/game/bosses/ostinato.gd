class_name Ostinato
extends Boss
## OSTINATO, Conductor of the Threshold: a crown of rings around a prism heart. Everything it plays repeats.
## Its rings align every other bar: that is when the heart is open.

var spiral := 0.0
## 0..1: how aligned the rings are (render, and damage taken).
var align := 0.0
var _beams: Array[Hazard] = []
var _gap := 0.0

func _init(_w: World) -> void:
	super(0.0, -6.0, 2000.0, 2.6)
	kind = "ostinato"; look = "ostinato"; boss_name = "ostinato"
	color = P.MAGENTA

func move(w: World, k: float) -> void:
	var tt := w.beat.beat * (PI / 16.0)
	var tx := sin(tt) * 5.0
	var ty := -6.0 + cos(tt * 2.0) * 2.0
	vx += (tx - x) * 0.02 * k
	vy += (ty - y) * 0.02 * k
	vx *= 0.9
	vy *= 0.9
	var open := int(floorf(w.beat.beat / 4.0)) % 2 == 1 and state == "fight"
	align += ((1.0 if open else 0.0) - align) * 0.1
	armor = 0.5 + align * 0.9
	spin += 0.01 + (1.0 - align) * 0.02
	for i in _beams.size():
		var h := _beams[i]
		var a := spin * 1.4 + (float(i) / float(_beams.size())) * U.TAU_F
		h.x = x + cos(a) * (r + 0.5)
		h.y = y + sin(a) * (r + 0.5)
		h.x2 = x + cos(a) * 26.0
		h.y2 = y + sin(a) * 26.0

func on_phase(w: World, ph: int) -> void:
	if ph == 2:
		for i in 2:
			var h := Hazard.new("beam", x, y, x, y, 0.3, U.jround(w.beat.spb * 60.0 * 4.0), P.MAGENTA, 1e9)
			_beams.append(h)
			w.add_hazard(h)
	if ph == 3:
		_beams.clear()

func attack(w: World, ph: int, bar: int, step: int) -> void:
	var aim := angle_to(w)
	if ph == 1:
		if step % 4 == 0:
			P.ring(w, x, y, 14, 7.0, spiral, {"color": P.MAGENTA})
			spiral += 0.14
			w.sfx("enemyShot", 0.7, 0.4)
		if bar % 2 == 1 and step == 8:
			P.fan(w, x, y, aim, 3, 0.5, 9.5, {"color": P.WHITE, "kind": EnemyBullets.BIG, "r": 0.5})
		if bar % 4 == 3 and step % 2 == 0:
			for i in 4:
				var a := (float(i) / 4.0) * U.TAU_F + bar * 0.4 + step * 0.06
				w.bullets.add(x + cos(a) * r, y + sin(a) * r, a, 12.0, {"color": P.VIOLET, "kind": EnemyBullets.SHARD, "r": 0.28})
	elif ph == 2:
		if step % 4 == 0:
			P.ring(w, x, y, 10, 6.5, spiral, {"color": P.MAGENTA})
			P.ring(w, x, y, 10, 6.5, -spiral * 1.3, {"color": P.VIOLET})
			spiral += 0.18
		if step == 0 or step == 6 or step == 12:
			P.fan(w, x, y, aim, 5, 0.7, 10.0, {"color": P.WHITE, "kind": EnemyBullets.BIG, "r": 0.45})
	else:
		# The loop with a gap: rings on eighths, a hole that drifts — find it, stay in it.
		if step % 2 == 0:
			var n := 28
			for i in n:
				var a := (float(i) / float(n)) * U.TAU_F
				var d := absf(fmod(a - _gap + PI * 3.0, U.TAU_F) - PI)
				if d < 0.55:
					continue
				w.bullets.add(x, y, a, 6.0, {"color": P.MAGENTA if i % 2 == 1 else P.VIOLET, "r": 0.3})
			_gap += 0.09 * (1.0 if bar % 4 < 2 else -1.0)
		if step in [0, 3, 6, 10, 12]:
			P.fan(w, x, y, aim, 2, 0.12, 13.0, {"color": P.WHITE, "kind": EnemyBullets.SHARD, "r": 0.3})
		if bar % 2 == 0 and step == 0:
			for i in 4:
				var e := w.arena.edge((float(i) / 4.0) * U.TAU_F + bar, 3.0)
				var m := w.add(Mote.new(e.x, e.y))
				m.spawn_t = 40
				w.emit({"t": "spawn", "x": e.x, "y": e.y, "color": m.color, "size": m.r})
