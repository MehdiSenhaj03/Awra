class_name Glyphs
## The twelve glyphs and the recogniser: a $P point-cloud matcher (Vatavu, Anthony, Wobbrock), so a glyph may be
## drawn in one or several strokes, in any order and direction. Plus the "step" gesture: one straight vertical
## stroke moves the mage up or down a lane.

enum { FIRE, WATER, WIND, BOLT, EARTH, SHIELD, WALL, STAR, HEAL, ICE, TIME, SEAL }
const COUNT := 12
const NAMES := ["fire", "water", "wind", "bolt", "earth", "shield", "wall", "star", "heal", "ice", "time", "seal"]
const N := 32
const REJECT := 0.46       # score under which a drawing fizzles

static var _templates: Array = []      # [{g, pts: PackedVector2Array}]

## Template strokes, in a 0..1 box (y down), possibly several variants per glyph.
static func shapes(g: int) -> Array:
	match g:
		FIRE:
			return [
				[[Vector2(0.0, 1.0), Vector2(0.2, 0.0), Vector2(0.4, 0.75), Vector2(0.6, 0.0), Vector2(0.8, 0.75), Vector2(1.0, 0.0)]],
				[[Vector2(0.0, 1.0), Vector2(0.25, 0.0), Vector2(0.5, 0.8), Vector2(0.75, 0.0), Vector2(1.0, 1.0)]],
				[[Vector2(0.0, 0.7), Vector2(0.25, 0.3), Vector2(0.5, 0.7), Vector2(0.75, 0.3), Vector2(1.0, 0.7)]],
			]
		WATER:
			return [[_wave(2.0, 0.0)], [_wave(2.0, PI)], [_wave(1.5, 0.0)]]
		WIND:
			return [[_spiral(1.0)], [_spiral(-1.0)]]
		BOLT:
			return [
				[[Vector2(0.65, 0.0), Vector2(0.2, 0.5), Vector2(0.8, 0.5), Vector2(0.35, 1.0)]],
				[[Vector2(0.35, 0.0), Vector2(0.8, 0.5), Vector2(0.2, 0.5), Vector2(0.65, 1.0)]],
			]
		EARTH:
			return [[[Vector2(0.5, 0.0), Vector2(1.0, 0.9), Vector2(0.0, 0.9), Vector2(0.5, 0.0)]]]
		SHIELD:
			return [[_circle(1.0, 0.5, 0.5, 0.5, 0.5)]]
		WALL:
			return [[[Vector2(0, 0), Vector2(1, 0), Vector2(1, 1), Vector2(0, 1), Vector2(0, 0)]]]
		STAR:
			var s: Array = []
			for k in 6:
				var a := -PI / 2 + k * TAU * 2.0 / 5.0
				s.append(Vector2(0.5 + cos(a) * 0.5, 0.5 + sin(a) * 0.5))
			return [[s]]
		HEAL:
			return [[[Vector2(0.5, 0.0), Vector2(0.5, 1.0)], [Vector2(0.0, 0.5), Vector2(1.0, 0.5)]]]
		ICE:
			return [[[Vector2(0.5, 0.0), Vector2(0.5, 1.0)], [Vector2(0.07, 0.25), Vector2(0.93, 0.75)], [Vector2(0.93, 0.25), Vector2(0.07, 0.75)]]]
		TIME:
			var l: Array = []
			for k in 41:
				var t := TAU * k / 40.0
				l.append(Vector2(0.5 + 0.5 * sin(t), 0.5 + 0.28 * sin(2.0 * t)))
			return [[l]]
		SEAL:
			return [[[Vector2(0.5, 0.0), Vector2(0.85, 0.5), Vector2(0.5, 1.0), Vector2(0.15, 0.5), Vector2(0.5, 0.0)]]]
	return []

static func _wave(periods: float, ph: float) -> Array:
	var out: Array = []
	for k in 33:
		var t := k / 32.0
		out.append(Vector2(t, 0.5 - 0.22 * sin(ph + t * TAU * periods)))
	return out

static func _spiral(dir: float) -> Array:
	var out: Array = []
	for k in 49:
		var t := k / 48.0
		var a := dir * t * TAU * 1.6
		var r := 0.5 * (1.0 - t * 0.85)
		out.append(Vector2(0.5 + cos(a) * r, 0.5 + sin(a) * r))
	return out

static func _circle(dir: float, cx: float, cy: float, rx: float, ry: float) -> Array:
	var out: Array = []
	for k in 33:
		var a := dir * TAU * k / 32.0
		out.append(Vector2(cx + cos(a) * rx, cy + sin(a) * ry))
	return out

static func _ensure() -> void:
	if not _templates.is_empty():
		return
	for g in COUNT:
		for v: Array in shapes(g):
			_templates.append({"g": g, "pts": normalize(v), "corners": corners(v), "strokes": v.size()})

## Sharp corners of a drawing (closed strokes wrap around): circles have none, squares four, stars five.
## A tie-breaker for shapes the point cloud alone confuses. Works at a scale relative to the drawing's size.
static func corners(strokes: Array) -> int:
	var mn := Vector2(INF, INF)
	var mx := Vector2(-INF, -INF)
	for st: Array in strokes:
		for p: Vector2 in st:
			mn = mn.min(p)
			mx = mx.max(p)
	var diag := maxf((mx - mn).length(), 1e-6)
	var count := 0
	for st: Array in strokes:
		if st.size() < 2:
			continue
		# Points every diag/28 along the stroke, lightly smoothed.
		var path := 0.0
		for i in range(1, st.size()):
			path += (st[i] as Vector2).distance_to(st[i - 1])
		var n := clampi(int(path / (diag / 28.0)), 4, 200)
		var raw := _resample([st], n)
		var pts := PackedVector2Array()
		pts.resize(raw.size())
		for i in raw.size():
			pts[i] = (raw[maxi(0, i - 1)] + raw[i] * 2.0 + raw[mini(raw.size() - 1, i + 1)]) * 0.25
		var closed: bool = (st[0] as Vector2).distance_to(st[st.size() - 1]) < diag * 0.2
		# Drop repeated points (the resampler pads its end), and for a closed stroke the tail that comes back
		# onto the start.
		var clean := PackedVector2Array()
		for q in pts:
			if clean.is_empty() or clean[clean.size() - 1].distance_to(q) > diag * 0.004:
				clean.append(q)
		if closed:
			while clean.size() > 6 and clean[clean.size() - 1].distance_to(clean[0]) < diag / 56.0:
				clean.remove_at(clean.size() - 1)
		pts = clean
		n = pts.size()
		if n < 5:
			continue
		var ang := PackedFloat32Array()
		ang.resize(n)
		for i in n:
			var ia := i - 2
			var ib := i + 2
			if closed:
				ia = (ia + n) % n
				ib = ib % n
			elif ia < 0 or ib >= n:
				continue
			var u := pts[i] - pts[ia]
			var v := pts[ib] - pts[i]
			if u.length() < 1e-6 or v.length() < 1e-6:
				continue
			ang[i] = absf(u.angle_to(v))
		for i in n:
			if ang[i] < deg_to_rad(50.0):
				continue
			var peak := true
			for k in range(-2, 3):
				var j := i + k
				if closed:
					j = (j + n) % n
				elif j < 0 or j >= n:
					continue
				if j != i and (ang[j] > ang[i] or (ang[j] == ang[i] and j < i)):
					peak = false
					break
			if peak:
				count += 1
	return count

# ---- $P ------------------------------------------------------------------------------------------------------------

## Strokes (arrays of Vector2) to N points: resampled along the path, scaled to a unit box, centred.
static func normalize(strokes: Array) -> PackedVector2Array:
	var pts := _resample(strokes, N)
	# Scale.
	var mn := Vector2(INF, INF)
	var mx := Vector2(-INF, -INF)
	for p in pts:
		mn = mn.min(p)
		mx = mx.max(p)
	var s := maxf(mx.x - mn.x, mx.y - mn.y)
	if s < 1e-6:
		s = 1.0
	var c := Vector2.ZERO
	for i in pts.size():
		pts[i] = (pts[i] - mn) / s
		c += pts[i]
	c /= pts.size()
	for i in pts.size():
		pts[i] -= c
	return pts

static func _resample(strokes: Array, n: int) -> PackedVector2Array:
	var total := 0.0
	for st: Array in strokes:
		for i in range(1, st.size()):
			total += (st[i] as Vector2).distance_to(st[i - 1])
	var out := PackedVector2Array()
	if total < 1e-6:
		for st: Array in strokes:
			for p: Vector2 in st:
				out.append(p)
		while out.size() < n and out.size() > 0:
			out.append(out[0])
		return out
	var step := total / (n - 1)
	var acc := 0.0
	for st: Array in strokes:
		if st.is_empty():
			continue
		var pts: Array = st.duplicate()
		out.append(pts[0])
		var i := 1
		while i < pts.size():
			var a: Vector2 = pts[i - 1]
			var b: Vector2 = pts[i]
			var d := a.distance_to(b)
			if acc + d >= step and d > 0.0:
				var q := a + (b - a) * ((step - acc) / d)
				out.append(q)
				pts.insert(i, q)
				acc = 0.0
			else:
				acc += d
			i += 1
	while out.size() > n:
		out.remove_at(out.size() - 1)
	while out.size() < n:
		out.append(out[out.size() - 1])
	return out

static func _cloud_distance(a: PackedVector2Array, b: PackedVector2Array, start: int) -> float:
	var n := a.size()
	var matched := PackedByteArray()
	matched.resize(n)
	var sum := 0.0
	var i := start
	for k in n:
		var best := INF
		var idx := -1
		for j in n:
			if matched[j] == 0:
				var d := a[i].distance_squared_to(b[j])
				if d < best:
					best = d
					idx = j
		matched[idx] = 1
		var weight := 1.0 - float(k) / n
		sum += weight * sqrt(best)
		i = (i + 1) % n
	return sum

static func _greedy(a: PackedVector2Array, b: PackedVector2Array) -> float:
	var n := a.size()
	var step := int(floor(pow(n, 0.5)))
	var best := INF
	var i := 0
	while i < n:
		best = minf(best, minf(_cloud_distance(a, b, i), _cloud_distance(b, a, i)))
		i += step
	return best

## Recognise drawn strokes. Returns {g: glyph or -1, score: 0..1, second: next best glyph's score}.
static func recognize(strokes: Array) -> Dictionary:
	_ensure()
	var pts := normalize(strokes)
	var cn := corners(strokes)
	var best := INF
	var best_g := -1
	var per := {}
	for t: Dictionary in _templates:
		var d := _greedy(pts, t.pts) + absf(cn - int(t.corners)) * 0.35 + absf(strokes.size() - int(t.strokes)) * 0.6
		var g: int = t.g
		if not per.has(g) or d < float(per[g]):
			per[g] = d
		if d < best:
			best = d
			best_g = g
	var second := INF
	for g: int in per:
		if g != best_g:
			second = minf(second, float(per[g]))
	var score := clampf(1.0 - best / 5.0, 0.0, 1.0)
	var s2 := clampf(1.0 - second / 5.0, 0.0, 1.0)
	# Too far from every glyph, or torn between two: it fizzles.
	if score < REJECT or (best > 2.1 and second - best < 0.08):
		best_g = -1
	return {"g": best_g, "score": score, "second": s2, "d": best}

## A straight, mostly vertical single stroke: +1 down, -1 up, 0 not a step.
static func step_of(strokes: Array, min_len: float) -> int:
	if strokes.size() != 1:
		return 0
	var st: Array = strokes[0]
	if st.size() < 2:
		return 0
	var a: Vector2 = st[0]
	var b: Vector2 = st[st.size() - 1]
	var path := 0.0
	for i in range(1, st.size()):
		path += (st[i] as Vector2).distance_to(st[i - 1])
	var chord := a.distance_to(b)
	if chord < min_len or chord / maxf(path, 1e-6) < 0.9:
		return 0
	var d := b - a
	if absf(d.y) < absf(d.x) * 2.2:
		return 0
	return 1 if d.y > 0.0 else -1

## Template points (unit box) of a glyph's first variant, for drawing it.
static func outline(g: int) -> Array:
	var s: Array = shapes(g)
	return s[0] if s.size() > 0 else []
