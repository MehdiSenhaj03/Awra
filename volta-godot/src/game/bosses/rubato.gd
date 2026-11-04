class_name Rubato
extends Boss
## RUBATO, Conductor of the Mercury Ocean: a serpent of liquid chrome that steals time. Each phase changes the
## tempo; it dives and resurfaces where its wake glows.

var trail: Array[Vector2] = []
var segments: Array[Segment] = []
var _path := 0.0
var _dive := 0
var _surface: Hazard = null

func _init(w: World) -> void:
	super(0.0, -10.0, 2400.0, 1.5)
	kind = "rubato"; look = "rubato"; boss_name = "rubato"
	color = P.VIOLET
	for i in 14:
		segments.append(w.add(Segment.new(self, i)))

func move(w: World, k: float) -> void:
	_path += 0.011 * k * (w.beat.bpm / 118.0)
	var a := _path
	var tx := sin(a) * 14.0
	var ty := sin(a * 2.0) * 8.0 - 2.0
	if _dive > 0:
		vis += (0.0 - vis) * 0.15
		hurtable = false
		targetable = false
		if _surface:
			tx = _surface.x
			ty = _surface.y
	else:
		vis += (1.0 - vis) * 0.15
		if state == "fight":
			hurtable = true
			targetable = true
	vx += (tx - x) * 0.06 * k
	vy += (ty - y) * 0.06 * k
	vx *= 0.85
	vy *= 0.85
	rot = atan2(vy, vx)
	contact = vis > 0.5
	trail.push_front(Vector2(x, y))
	if trail.size() > 120:
		trail.pop_back()

func on_phase(w: World, ph: int) -> void:
	w.beat.set_bpm(96.0 if ph == 2 else 146.0)

func attack(w: World, ph: int, bar: int, step: int) -> void:
	var aim := angle_to(w)
	if _dive > 0:
		if step == 0:
			_dive -= 1
			if _dive == 0:
				P.ring(w, x, y, 24, 7.0, 0.0, {"color": P.VIOLET, "r": 0.32})
				P.ring(w, x, y, 24, 5.0, U.TAU_F / 48.0, {"color": P.WHITE, "r": 0.28})
				w.emit({"t": "boom", "x": x, "y": y, "r": 4.0, "color": P.VIOLET})
				w.emit({"t": "shake", "a": 0.5})
				w.sfx("bigBoom")
				if _surface:
					_surface.done = true
				_surface = null
		return
	if step % 4 == 0:
		P.fan(w, x, y, aim, 5 if ph == 3 else 3, 0.4, 10.0, {"color": P.WHITE, "kind": EnemyBullets.SHARD, "r": 0.3})
	# A ripple runs down the body every bar.
	if step % 2 == 0:
		var s := segments[(step / 2 + bar * 3) % segments.size()]
		var turn := 0.0
		if ph == 3:
			turn = 0.6 if step % 4 != 0 else -0.6
		P.ring(w, s.x, s.y, 4 if ph == 1 else 6, 4.5, step * 0.3, {"color": P.VIOLET, "r": 0.28, "turn": turn})
	if ph >= 2 and bar % 4 == 3 and step == 0:
		var p := w.player
		_surface = Hazard.new("zone", p.x + p.vx * 0.5, p.y + p.vy * 0.5, x, y, 4.0, U.jround(w.beat.spb * 60.0 * 4.0), P.VIOLET, 0)
		w.add_hazard(_surface)
		_dive = 1
		w.sfx("phase")
	if ph >= 2 and step == 8:
		for i in segments.size():
			if i % 3 == bar % 3:
				var s := segments[i]
				w.bullets.add(s.x, s.y, w.rng.range_f(0.0, U.TAU_F), 0.5, {"color": P.VIOLET, "wait": 1.2, "r": 0.36, "life": 6.0})

func on_death(w: World) -> void:
	w.beat.set_bpm(118.0)
	for s in segments:
		w.emit({"t": "boom", "x": s.x, "y": s.y, "r": 2.0, "color": P.VIOLET})

func release() -> void:
	segments.clear()
	_surface = null
