class_name Weaver
extends Enemy
## Tisseuse: sweeps beams of light around itself, one revolution every two bars.

var beams: Array[Hazard] = []
var a := 0.0

func _init(px_: float, py_: float) -> void:
	super(px_, py_)
	kind = "weaver"; look = "weaver"
	r = 1.0; hp = 20.0; max_hp = 20.0; value = 340.0; notes = 5; heavy = true
	color = P.CYAN

func update(w: World, _k: float) -> void:
	steer(w.player.x * 0.2, w.player.y * 0.2, 1.0, 0.01)
	if beams.is_empty() and t > 30.0:
		for i in (3 if elite else 2):
			var h := Hazard.new("beam", x, y, x, y, 0.28, U.jround(w.beat.spb * 60.0 * 2.0), P.CYAN, 1e9)
			beams.append(h)
			w.add_hazard(h)
	a += (U.TAU_F / (w.beat.spb * 60.0 * 8.0)) * (1.2 if elite else 1.0) * w.k
	spin = a
	var length := 9.0
	for i in beams.size():
		var h := beams[i]
		var b := a + (float(i) / float(beams.size())) * U.TAU_F
		h.x = x + cos(b) * r
		h.y = y + sin(b) * r
		h.x2 = x + cos(b) * (r + length)
		h.y2 = y + sin(b) * (r + length)

func on_death(_w: World) -> void:
	for h in beams:
		h.done = true

func release() -> void:
	beams.clear()
