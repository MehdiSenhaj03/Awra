class_name Crescendo
extends Boss
## CRESCENDO, Conductor of the Glass Garden: a tree of crystal that only knows how to get louder.

var _branches: Array[Hazard] = []
var _curtain := 0.0

func _init(_w: World) -> void:
	super(0.0, -9.0, 2300.0, 2.4)
	kind = "crescendo"; look = "crescendo"; boss_name = "crescendo"
	color = P.GREEN

func move(w: World, k: float) -> void:
	vx = (sin(w.beat.beat * 0.4) * 2.0 - x) * 0.8 * k
	vy = (-9.0 - y) * 0.8
	spin += 0.004
	var grow := minf(1.0, (fmod(bars(w), 4.0) / 4.0) * 1.2) if phase == 2 and state == "fight" else 0.0
	for i in _branches.size():
		var h := _branches[i]
		var a := PI / 2.0 + (i - 1) * 0.65 + sin(w.beat.beat * 0.25 + i) * 0.35
		var length := 4.0 + grow * 20.0
		h.x = x + cos(a) * r
		h.y = y + sin(a) * r
		h.x2 = x + cos(a) * (r + length)
		h.y2 = y + sin(a) * (r + length)

func on_phase(w: World, ph: int) -> void:
	if ph == 2:
		for i in 3:
			var h := Hazard.new("beam", x, y, x, y, 0.34, U.jround(w.beat.spb * 60.0 * 2.0), P.GREEN, 1e9)
			_branches.append(h)
			w.add_hazard(h)
	if ph == 3:
		_branches.clear()

func attack(w: World, ph: int, bar: int, step: int) -> void:
	var aim := angle_to(w)
	var swell := bar % 8
	if step % 4 == 0:
		var n := 3 + swell + (ph - 1) * 2
		P.fan(w, x, y, aim, n, 0.35 + swell * 0.08, 3.5, {"color": P.GREEN, "acc": 6.0 + ph * 2.0, "r": 0.3})
		w.sfx("enemyShot", 0.9 + swell * 0.05, 0.4)
	if step == 8 and bar % 2 == 0:
		for i in (2 if ph == 1 else 3):
			var a := w.rng.range_f(0.0, U.TAU_F)
			var d := w.rng.range_f(6.0, 13.0)
			var c := w.add(Crystal.new(cos(a) * d, sin(a) * d * 0.8))
			c.spawn_t = 30
			w.emit({"t": "spawn", "x": c.x, "y": c.y, "color": P.GREEN, "size": 0.9})
	if ph >= 2 and step == 12:
		P.ring(w, x, y, 18 + swell * 2, 5.0, bar * 0.3, {"color": P.CYAN, "r": 0.3, "acc": 2.0})
	if ph == 3 and step % 2 == 0:
		var n := 18
		for i in n:
			var pxx := -18.0 + (float(i) / float(n - 1)) * 36.0
			if absf(pxx - sin(_curtain) * 12.0) < 3.2:
				continue
			if step % 4 == 0:
				w.bullets.add(pxx, -19.0, PI / 2.0, 7.0 + swell * 0.4, {"color": P.GREEN, "kind": EnemyBullets.SHARD, "r": 0.26})
		_curtain += 0.12
