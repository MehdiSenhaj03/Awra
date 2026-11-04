class_name Hazard
extends RefCounted
## Telegraphs and area attacks: warning lines, laser beams, landing zones. "line" only warns; "beam" warns
## for `delay` frames then burns for `n` frames; "zone" resolves after `delay` (boom, hit, n radial bullets).

var id: int = 0
var t: float = 0.0
var done := false
var kind: String
var x: float
var y: float
var x2: float
var y2: float
var r: float
var delay: float
var color: int
var n: float

func _init(k: String, px: float, py: float, px2: float, py2: float, rad: float, d: float, c: int = 0xff2fb9, count: float = 0.0) -> void:
	kind = k; x = px; y = py; x2 = px2; y2 = py2; r = rad; delay = d; color = c; n = count

var k: float:
	get: return minf(1.0, t / maxf(1.0, delay))

var live: bool:
	get: return kind == "beam" and t >= delay

static func seg_dist(px: float, py: float, x1: float, y1: float, x2_: float, y2_: float) -> float:
	var dx := x2_ - x1
	var dy := y2_ - y1
	var l2 := dx * dx + dy * dy
	if l2 == 0.0:
		l2 = 1.0
	var tt := maxf(0.0, minf(1.0, ((px - x1) * dx + (py - y1) * dy) / l2))
	return U.hypot(px - (x1 + dx * tt), py - (y1 + dy * tt))

func update(w: World) -> void:
	t += w.k
	var p := w.player
	if kind == "zone":
		if t >= delay:
			done = true
			w.emit({"t": "boom", "x": x, "y": y, "r": r, "color": color})
			w.emit({"t": "shake", "a": 0.25})
			w.sfx("mortar")
			if U.hypot(p.x - x, p.y - y) < r + 0.2:
				w.hurt_player(x, y)
			for i in int(n):
				w.bullets.add(x, y, (float(i) / n) * U.TAU_F, 7.0, {"color": color, "r": 0.3})
	elif kind == "line":
		if t >= delay:
			done = true
	elif kind == "beam":
		if t >= delay and seg_dist(p.x, p.y, x, y, x2, y2) < r + 0.3:
			w.hurt_player(x, y)
		if t >= delay + n:
			done = true
