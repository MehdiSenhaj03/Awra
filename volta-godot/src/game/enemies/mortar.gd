class_name Mortar
extends Enemy
## Cloche: lobs shells that land on the next bar, then burst.

func _init(px_: float, py_: float) -> void:
	super(px_, py_)
	kind = "mortar"; look = "mortar"
	r = 0.9; hp = 11.0; max_hp = 11.0; value = 200.0; notes = 3; heavy = true
	color = P.ORANGE

func update(w: World, _k: float) -> void:
	steer(-w.player.x * 0.3, -w.player.y * 0.3, 1.2, 0.02)
	rot += 0.01
	if w.beat.crossed(1) and w.beat.index(1) % 8 == 0 and t > 30.0:
		var n := 4 if elite else 3
		var p := w.player
		for i in n:
			var a := w.rng.range_f(0.0, U.TAU_F)
			var d := 0.0 if i == 0 else w.rng.range_f(2.5, 5.5)
			var hx := p.x + p.vx * 0.4 + cos(a) * d
			var hy := p.y + p.vy * 0.4 + sin(a) * d
			w.add_hazard(Hazard.new("zone", hx, hy, x, y, 2.1, U.jround(w.beat.spb * 60.0 * 4.0), P.ORANGE, 6))
		w.emit({"t": "ring", "x": x, "y": y, "r": 1.6, "color": P.ORANGE, "life": 16})
		w.sfx("mortarLaunch")
