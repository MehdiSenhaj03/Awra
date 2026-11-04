class_name Splitter
extends Enemy
## Prisme: splits into smaller copies twice.

var gen: int

func _init(px_: float, py_: float, g: int = 0) -> void:
	super(px_, py_)
	gen = g
	kind = "splitter"; look = "splitter"
	r = [1.0, 0.7, 0.48][g]
	hp = [10.0, 4.0, 2.0][g]; max_hp = hp
	value = [90.0, 50.0, 30.0][g]
	notes = 1
	color = P.CYAN
	if g > 0:
		spawn_t = 0

func update(w: World, _k: float) -> void:
	steer(w.player.x, w.player.y, [2.6, 4.2, 5.8][gen], 0.04)
	rot += 0.03 + gen * 0.03
	if gen == 0 and w.beat.crossed(1) and w.beat.index(1) % 4 == 0 and t > 60.0:
		P.ring(w, x, y, 8, 6.0, rot, {"color": P.CYAN})

func on_death(w: World) -> void:
	if gen >= 2:
		return
	for i in 3:
		var a := (float(i) / 3.0) * U.TAU_F + rot
		var s: Splitter = w.add(Splitter.new(x + cos(a) * 0.6, y + sin(a) * 0.6, gen + 1))
		s.vx = cos(a) * 9.0
		s.vy = sin(a) * 9.0
