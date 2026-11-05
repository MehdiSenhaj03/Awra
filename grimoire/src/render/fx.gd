class_name Fx2
extends Node2D
## Short-lived effects drawn in one pass: sparks and ink droplets, rings, lightning, glyph flashes, floating text.

var _p: Array = []          # particles {pos, vel, col, life, max, size, drag, grav}
var _rings: Array = []      # {pos, r0, r1, col, life, max, width}
var _bolts: Array = []      # {pts, col, life, max, width}
var _glyphs: Array = []     # {g, pos, size, col, life, max}
var _texts: Array = []      # {text, pos, col, life, max, size}
var shake := 0.0
var _font: Font

func _ready() -> void:
	_font = Kit.font("deco", 400, 0.0, 30)
	var m := CanvasItemMaterial.new()
	m.blend_mode = CanvasItemMaterial.BLEND_MODE_ADD
	material = m

func burst(pos: Vector2, col: Color, n: int = 24, speed: float = 420.0, size: float = 5.0, life: float = 0.7, grav: float = 300.0) -> void:
	for i in n:
		var a := randf() * TAU
		var sp := speed * (0.3 + randf() * 0.7)
		_p.append({"pos": pos, "vel": Vector2(cos(a), sin(a)) * sp, "col": col, "life": life * (0.6 + randf() * 0.5), "max": life, "size": size * (0.5 + randf()), "drag": 2.2, "grav": grav})

func trail(pos: Vector2, col: Color, vel: Vector2, size: float = 4.0, life: float = 0.45) -> void:
	_p.append({"pos": pos + Vector2(randf_range(-6, 6), randf_range(-6, 6)), "vel": vel + Vector2(randf_range(-40, 40), randf_range(-60, 20)), "col": col, "life": life, "max": life, "size": size, "drag": 3.0, "grav": -60.0})

func ring(pos: Vector2, col: Color, r0: float = 10.0, r1: float = 140.0, life: float = 0.5, width: float = 6.0) -> void:
	_rings.append({"pos": pos, "r0": r0, "r1": r1, "col": col, "life": life, "max": life, "width": width})

func lightning(a: Vector2, b: Vector2, col: Color, life: float = 0.35, width: float = 5.0) -> void:
	var pts := PackedVector2Array([a])
	var n := int(a.distance_to(b) / 40.0) + 2
	var nrm := (b - a).orthogonal().normalized()
	for i in range(1, n):
		var t := float(i) / n
		pts.append(a.lerp(b, t) + nrm * randf_range(-34, 34))
	pts.append(b)
	_bolts.append({"pts": pts, "col": col, "life": life, "max": life, "width": width})

func glyph(g: int, pos: Vector2, size: float, col: Color, life: float = 0.8) -> void:
	_glyphs.append({"g": g, "pos": pos, "size": size, "col": col, "life": life, "max": life})

func text(t: String, pos: Vector2, col: Color, size: int = 30, life: float = 1.1) -> void:
	_texts.append({"text": t, "pos": pos, "col": col, "life": life, "max": life, "size": size})

func _process(dt: float) -> void:
	for p: Dictionary in _p:
		p.life -= dt
		p.vel = (p.vel as Vector2) * exp(-float(p.drag) * dt) + Vector2(0, float(p.grav) * dt)
		p.pos = (p.pos as Vector2) + (p.vel as Vector2) * dt
	_p = _p.filter(func(p: Dictionary) -> bool: return p.life > 0.0)
	for arr: Array in [_rings, _bolts, _glyphs, _texts]:
		for e: Dictionary in arr:
			e.life -= dt
	_rings = _rings.filter(func(e: Dictionary) -> bool: return e.life > 0.0)
	_bolts = _bolts.filter(func(e: Dictionary) -> bool: return e.life > 0.0)
	_glyphs = _glyphs.filter(func(e: Dictionary) -> bool: return e.life > 0.0)
	_texts = _texts.filter(func(e: Dictionary) -> bool: return e.life > 0.0)
	shake = maxf(0.0, shake - dt * 2.5)
	queue_redraw()

func _draw() -> void:
	for p: Dictionary in _p:
		var k: float = clampf(p.life / p.max, 0.0, 1.0)
		var c: Color = p.col
		draw_circle(p.pos, float(p.size) * (0.4 + 0.6 * k), Color(c.r, c.g, c.b, c.a * k))
	for e: Dictionary in _rings:
		var k: float = 1.0 - clampf(e.life / e.max, 0.0, 1.0)
		var c: Color = e.col
		var r := lerpf(e.r0, e.r1, ease(k, 0.4))
		draw_arc(e.pos, r, 0, TAU, 48, Color(c.r, c.g, c.b, c.a * (1.0 - k)), float(e.width) * (1.0 - k * 0.6), true)
	for e: Dictionary in _bolts:
		var k: float = clampf(e.life / e.max, 0.0, 1.0)
		var c: Color = e.col
		draw_polyline(e.pts, Color(c.r, c.g, c.b, 0.25 * k), float(e.width) * 4.0, true)
		draw_polyline(e.pts, Color(c.r, c.g, c.b, k), e.width, true)
		draw_polyline(e.pts, Color(1, 1, 1, k), float(e.width) * 0.35, true)
	for e: Dictionary in _glyphs:
		var k: float = clampf(e.life / e.max, 0.0, 1.0)
		var c: Color = e.col
		var s: float = float(e.size) * (1.0 + (1.0 - k) * 0.35)
		GlyphArt.draw(self, e.g, e.pos, s, Color(c.r, c.g, c.b, k), 5.0)
	for e: Dictionary in _texts:
		var k: float = clampf(e.life / e.max, 0.0, 1.0)
		var c: Color = e.col
		var pos: Vector2 = (e.pos as Vector2) + Vector2(0, -40.0 * (1.0 - k))
		draw_string(_font, pos - Vector2(200, 0), e.text, HORIZONTAL_ALIGNMENT_CENTER, 400, e.size, Color(c.r, c.g, c.b, minf(1.0, k * 2.0)))
