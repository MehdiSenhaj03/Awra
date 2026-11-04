class_name GoldenEcho
extends Enemy
## A golden echo carrying a memory: it flees, then vanishes. Kill it fast.

var fragment: String

func _init(px_: float, py_: float, frag: String) -> void:
	super(px_, py_)
	fragment = frag
	kind = "echo"; look = "echo"
	r = 0.55; hp = 9.0; max_hp = 9.0; value = 500.0; notes = 6
	color = P.GOLD
	contact = false

func update(w: World, _k: float) -> void:
	var a := angle_to(w) + PI + sin(t * 0.04) * 0.9
	steer(x + cos(a) * 5.0, y + sin(a) * 5.0, 8.0, 0.08)
	rot += 0.1
	if t > w.beat.spb * 60.0 * 12.0:
		vis -= 0.05
		hurtable = vis > 0.3
		if vis <= 0.0:
			dead = true

func on_death(w: World) -> void:
	w.drop(x, y, 1, "fragment", fragment)
