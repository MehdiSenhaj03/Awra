class_name Arena
extends RefCounted
## The playing field: a circle or a rectangle (half sizes), with pillars that stop bullets.

var shape: String
var r: float
var w: float
var h: float
## Each pillar: Vector3(x, y, radius).
var pillars: Array[Vector3] = []

func _init(d: Dictionary) -> void:
	shape = d.get("shape", "circle")
	r = d.get("r", 20.0)
	w = d.get("w", 22.0)
	h = d.get("h", 14.0)
	for p in d.get("pillars", []):
		pillars.append(Vector3(p[0], p[1], p[2]))

var extent: float:
	get: return r if shape == "circle" else maxf(w, h)

func inside(x: float, y: float, margin: float = 0.0) -> bool:
	if shape == "circle":
		return x * x + y * y <= (r - margin) * (r - margin)
	return absf(x) <= w - margin and absf(y) <= h - margin

## Keeps a disc inside the arena and out of pillars. Returns the corrected position in `out` (x, y) and true
## when it had to move.
func confine(px: float, py: float, rad: float) -> Vector3:
	var x := px
	var y := py
	var hit := false
	if shape == "circle":
		var d := sqrt(x * x + y * y)
		var m := r - rad
		if d > m:
			x *= m / d
			y *= m / d
			hit = true
	else:
		var cx := maxf(-w + rad, minf(w - rad, x))
		var cy := maxf(-h + rad, minf(h - rad, y))
		hit = cx != x or cy != y
		x = cx
		y = cy
	for q in pillars:
		var dx := x - q.x
		var dy := y - q.y
		var d := sqrt(dx * dx + dy * dy)
		var m := q.z + rad
		if d < m and d > 1e-6:
			x = q.x + (dx / d) * m
			y = q.y + (dy / d) * m
			hit = true
	return Vector3(x, y, 1.0 if hit else 0.0)

func blocked(x: float, y: float) -> bool:
	for q in pillars:
		if (x - q.x) * (x - q.x) + (y - q.y) * (y - q.y) < q.z * q.z:
			return true
	return false

func edge(a: float, inset: float = 2.0) -> Vector2:
	if shape == "circle":
		return Vector2(cos(a) * (r - inset), sin(a) * (r - inset))
	var c := cos(a)
	var s := sin(a)
	var k := minf((w - inset) / maxf(1e-6, absf(c)), (h - inset) / maxf(1e-6, absf(s)))
	return Vector2(c * k, s * k)
