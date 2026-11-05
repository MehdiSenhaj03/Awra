class_name GlyphArt
## Drawing glyphs: the clean calligraphic form of each one (from its template), whole or partly traced, with a
## soft glow. Used by the book, the HUD, the opponent's ghost glyph and the "recognised" flash.

const COLORS := [Color("#ff6a2b"), Color("#3f8fe0"), Color("#b6f0d2"), Color("#ffe26a"), Color("#b07a3c"), Color("#ffd27a"),
	Color("#e8d9b0"), Color("#fff1a8"), Color("#8ff08f"), Color("#bfeaff"), Color("#d6a8ff"), Color("#ff9ad0")]

## Draws glyph `g` centred at `c`, fitting a square of `size`, traced up to `progress` (0..1).
static func draw(ci: CanvasItem, g: int, c: Vector2, size: float, col: Color, width: float = 4.0, progress: float = 1.0, glow: bool = true) -> void:
	var strokes: Array = Glyphs.outline(g)
	if strokes.is_empty():
		return
	# Total length, to trace progressively.
	var total := 0.0
	for st: Array in strokes:
		for i in range(1, st.size()):
			total += (st[i] as Vector2).distance_to(st[i - 1])
	var left := total * clampf(progress, 0.0, 1.0)
	for st: Array in strokes:
		if left <= 0.0:
			break
		var pts := PackedVector2Array()
		pts.append(_map(st[0], c, size))
		for i in range(1, st.size()):
			var a: Vector2 = st[i - 1]
			var b: Vector2 = st[i]
			var d := a.distance_to(b)
			if d <= left:
				pts.append(_map(b, c, size))
				left -= d
			else:
				pts.append(_map(a.lerp(b, left / maxf(d, 1e-6)), c, size))
				left = 0.0
				break
		if pts.size() < 2:
			continue
		if glow:
			ci.draw_polyline(pts, Color(col.r, col.g, col.b, col.a * 0.18), width * 3.2, true)
			ci.draw_polyline(pts, Color(col.r, col.g, col.b, col.a * 0.35), width * 1.8, true)
		ci.draw_polyline(pts, col, width, true)
		# Round nib ends.
		ci.draw_circle(pts[0], width * 0.5, col)
		ci.draw_circle(pts[pts.size() - 1], width * 0.5, col)

static func _map(p: Vector2, c: Vector2, size: float) -> Vector2:
	return c + (p - Vector2(0.5, 0.5)) * size

## Ink strokes as drawn by a hand, with a glow (the pen's live trace).
static func ink(ci: CanvasItem, strokes: Array, col: Color, width: float = 7.0) -> void:
	for s in strokes:
		var pts: PackedVector2Array = s if s is PackedVector2Array else PackedVector2Array(s)
		if pts.size() < 2:
			if pts.size() == 1:
				ci.draw_circle(pts[0], width * 0.5, col)
			continue
		ci.draw_polyline(pts, Color(col.r, col.g, col.b, 0.14), width * 3.4, true)
		ci.draw_polyline(pts, Color(col.r, col.g, col.b, 0.3), width * 1.9, true)
		ci.draw_polyline(pts, col, width, true)
		ci.draw_polyline(pts, Color(1, 1, 1, 0.55), width * 0.3, true)
