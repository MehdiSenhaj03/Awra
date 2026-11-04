class_name Sniper
extends Enemy
## Œil: blinks to the edge, tracks with a laser for three beats, locks, fires a piercing column on the bar.

var beam: Hazard = null

func _init(px_: float, py_: float) -> void:
	super(px_, py_)
	kind = "sniper"; look = "sniper"
	r = 0.6; hp = 6.0; max_hp = 6.0; value = 190.0; notes = 2
	color = P.RED
	state = "move"

func update(w: World, _k: float) -> void:
	var spb := w.beat.spb * 60.0
	vx *= 0.9
	vy *= 0.9
	if state == "move":
		if st > spb * 2.0 and w.beat.crossed(1) and w.beat.index(1) % 4 == 0:
			beam = Hazard.new("line", x, y, 0.0, 0.0, 0.18, spb * 4.0, P.RED)
			w.add_hazard(beam)
			to("track")
			w.sfx("telegraph", 0.8, 0.5)
	elif state == "track":
		var lock := st > spb * 3.0
		if not lock:
			rot = angle_to(w)
		if beam:
			beam.x = x
			beam.y = y
			beam.x2 = x + cos(rot) * 60.0
			beam.y2 = y + sin(rot) * 60.0
			beam.r = 0.5 if lock else 0.12
		if st >= spb * 4.0:
			for i in (9 if elite else 6):
				w.bullets.add(x, y, rot, 30.0 + i * 2.2, {"color": P.RED, "kind": EnemyBullets.SHARD, "r": 0.34})
			w.emit({"t": "ring", "x": x, "y": y, "r": 1.6, "color": P.RED, "life": 10})
			w.sfx("sniper")
			to("relocate")
	elif state == "relocate":
		if st > spb:
			var e := w.arena.edge(w.rng.range_f(0.0, U.TAU_F), 3.0)
			w.emit({"t": "spawn", "x": x, "y": y, "color": P.RED, "size": 0.6})
			x = e.x
			y = e.y
			w.emit({"t": "spawn", "x": x, "y": y, "color": P.RED, "size": 0.6})
			to("move")

func release() -> void:
	beam = null
