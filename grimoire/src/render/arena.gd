class_name Arena
extends Control
## The duel on screen: the painted arena, the three lanes, both mages, every spell in flight with its own little
## animation, the shields and walls, and the effects of what happens (clashes, hits, lightning).

const LANE_Y := [520.0, 690.0, 860.0]
const FLY := -95.0                # spells fly at chest height above the lane line
const X0 := 300.0
const X1 := 1620.0
const SIDE_INK := [Color("#ffd27a"), Color("#ff6a5a")]

var duel: Duel
var fx: Fx2
var mages: Array[Sprite2D] = []
var mage_y := [690.0, 690.0]
var _flash := [0.0, 0.0]
var _cast := [0.0, 0.0]
var _t := 0.0
var hover_lane := -1
var ghost := {}              # the other side's glyph forming: {side, g, lane, progress}
var _layer: Node2D
var _glow: Node2D
var _bg: TextureRect

func _init(d: Duel, bg: String, left_tex: Texture2D, right_tex: Texture2D) -> void:
	duel = d
	Kit.full(self)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	_bg = Kit.background(self, bg, 0.0, 0.62)
	var shade := ColorRect.new()
	shade.material = ShaderMaterial.new()
	(shade.material as ShaderMaterial).shader = preload("res://src/render/shaders/vellum.gdshader")
	shade.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(shade)
	Kit.full(shade)
	_layer = Node2D.new()
	add_child(_layer)
	_layer.draw.connect(_draw_world)
	for i in 2:
		var s := Sprite2D.new()
		s.texture = left_tex if i == 0 else right_tex
		s.centered = true
		if s.texture:
			var h := 360.0
			s.scale = Vector2.ONE * h / s.texture.get_height()
		s.position = Vector2(X0 - 70 if i == 0 else X1 + 70, LANE_Y[1] - 110)
		add_child(s)
		mages.append(s)
	_glow = Node2D.new()
	var gm := CanvasItemMaterial.new()
	gm.blend_mode = CanvasItemMaterial.BLEND_MODE_ADD
	_glow.material = gm
	add_child(_glow)
	_glow.draw.connect(_draw_glow)
	fx = Fx2.new()
	add_child(fx)

static func sx(x: int) -> float:
	return lerpf(X0 + 40.0, X1 - 40.0, float(x) / Duel.LEN)

func mage_pos(side: int) -> Vector2:
	return Vector2(X0 - 70 if side == 0 else X1 + 70, mage_y[side] - 110)

func lane_of(y: float) -> int:
	var best := 0
	for i in 3:
		if absf(y - LANE_Y[i]) < absf(y - LANE_Y[best]):
			best = i
	return best

func flash(side: int) -> void:
	_flash[side] = 1.0

func cast_pose(side: int) -> void:
	_cast[side] = 1.0

func _process(dt: float) -> void:
	_t += dt
	for i in 2:
		var m := duel.mages[i]
		var want: float = LANE_Y[m.lane]
		mage_y[i] = lerpf(mage_y[i], want, 1.0 - exp(-dt * 14.0))
		var s := mages[i]
		var breathe := sin(_t * 1.6 + i) * 0.012
		var base := 360.0 / s.texture.get_height() if s.texture else 1.0
		var pose := ease(_cast[i], 0.5) * 0.06
		s.scale = Vector2(base * (1.0 + breathe + pose), base * (1.0 - breathe * 0.5 + pose))
		s.position = mage_pos(i) + Vector2((pose * 300.0) * (1 if i == 0 else -1), 0)
		var f: float = _flash[i]
		s.modulate = Color(1.0 + f * 1.5, 1.0 - f * 0.4, 1.0 - f * 0.4, 1.0)
		if m.ink_slow > 0:
			s.modulate *= Color(0.75, 0.9, 1.3)
		_flash[i] = maxf(0.0, f - dt * 5.0)
		_cast[i] = maxf(0.0, _cast[i] - dt * 3.0)
		if m.heal > 0 and randf() < dt * 30.0:
			fx.trail(mage_pos(i) + Vector2(randf_range(-60, 60), randf_range(-80, 120)), Color(0.5, 1.0, 0.5, 0.8), Vector2(0, -60), 5.0, 0.9)
	# Spell trails.
	for s in duel.spells:
		var p := Vector2(sx(s.x), LANE_Y[s.lane] + FLY)
		var dir := 1.0 if s.side == 0 else -1.0
		match s.kind:
			Glyphs.FIRE, Duel.FIRESTORM:
				if randf() < dt * 60.0:
					fx.trail(p, Color(1.0, 0.45 + randf() * 0.3, 0.1, 0.9), Vector2(-dir * 120, -30), 6.0 if s.kind == Glyphs.FIRE else 10.0)
			Duel.MAGMA:
				if randf() < dt * 40.0:
					fx.trail(p + Vector2(0, 20), Color(1.0, 0.3, 0.05, 0.9), Vector2(-dir * 60, -80), 7.0, 0.7)
			Glyphs.ICE, Duel.LANCE:
				if randf() < dt * 30.0:
					fx.trail(p, Color(0.7, 0.95, 1.0, 0.8), Vector2(-dir * 80, 0), 3.5)
			Glyphs.EARTH:
				if randf() < dt * 30.0:
					fx.trail(p + Vector2(0, 40), Color(0.75, 0.55, 0.3, 0.7), Vector2(-dir * 60, -50), 5.0, 0.6)
			Glyphs.STAR:
				if randf() < dt * 20.0:
					fx.trail(p, Color(1.0, 0.95, 0.6, 0.9), Vector2(-dir * 60, 0), 3.0, 0.4)
	_layer.queue_redraw()
	_glow.queue_redraw()
	position = Vector2(randf_range(-1, 1), randf_range(-1, 1)) * fx.shake * 14.0

func _draw_world() -> void:
	var ci := _layer
	# Lanes: dotted gold threads between the mages.
	for i in 3:
		var y: float = LANE_Y[i] + 40.0
		var hot := i == hover_lane
		var c := Color(1.0, 0.85, 0.5, 0.42 if hot else 0.16)
		var x := X0 + 20.0
		while x < X1 - 20.0:
			ci.draw_line(Vector2(x, y), Vector2(x + 18, y), c, 3.0 if hot else 2.0, true)
			x += 34.0
	# Walls.
	for s in duel.spells:
		if s.kind == Duel.WALLE or s.kind == Duel.ICEWALL:
			var p := Vector2(sx(s.x), LANE_Y[s.lane] - 40)
			var r := Rect2(p - Vector2(28, 120), Vector2(56, 170))
			if s.kind == Duel.ICEWALL:
				ci.draw_rect(r, Color(0.6, 0.9, 1.0, 0.55))
				ci.draw_rect(r, Color(0.85, 1.0, 1.0, 0.9), false, 3.0)
			else:
				ci.draw_rect(r, Color(0.86, 0.78, 0.6))
				for k in 5:
					var yy := r.position.y + 10 + k * 28
					ci.draw_line(Vector2(r.position.x, yy), Vector2(r.end.x, yy), Color(0.5, 0.4, 0.28), 2.0)
					var off := 13.0 if k % 2 == 0 else 26.0
					ci.draw_line(Vector2(r.position.x + off, yy), Vector2(r.position.x + off, yy + 28), Color(0.5, 0.4, 0.28), 2.0)
				ci.draw_rect(r, SIDE_INK[s.side] * Color(1, 1, 1, 0.9), false, 4.0)
				var dmg := 1.0 - clampf(float(s.hp) / 3000.0, 0.0, 1.0)
				if dmg > 0.3:
					ci.draw_polyline(PackedVector2Array([r.position + Vector2(10, 20), r.position + Vector2(30, 60), r.position + Vector2(18, 90), r.position + Vector2(40, 130)]), Color(0.2, 0.1, 0.05), 2.5)
	# Earth and magma: shards rising from the ground.
	for s in duel.spells:
		if s.kind == Glyphs.EARTH or s.kind == Duel.MAGMA:
			var p := Vector2(sx(s.x), LANE_Y[s.lane] + 45)
			var dir := 1.0 if s.side == 0 else -1.0
			var col := Color(0.55, 0.38, 0.22) if s.kind == Glyphs.EARTH else Color(0.35, 0.08, 0.04)
			for k in 4:
				var bx := p.x - dir * k * 26.0
				var h := (70.0 - k * 14.0) * (1.0 + 0.1 * sin(_t * 9.0 + k))
				var tri := PackedVector2Array([Vector2(bx - 16, p.y), Vector2(bx + dir * 4, p.y - h), Vector2(bx + 16, p.y)])
				ci.draw_colored_polygon(tri, col.lightened(0.1 * k))
				ci.draw_polyline(PackedVector2Array([tri[0], tri[1], tri[2]]), Color(0.15, 0.08, 0.04), 2.0)
	# The other side's glyph forming (read it!).
	if not ghost.is_empty():
		var side: int = ghost.side
		var gp := Vector2(X1 - 260.0 if side == 1 else X0 + 260.0, float(LANE_Y[int(ghost.lane)]) - 120.0)
		ci.draw_circle(gp, 100, Color(0.05, 0.02, 0.02, 0.35))
		GlyphArt.draw(ci, int(ghost.g), gp, 160.0, Color(0.1, 0.03, 0.05, 0.9), 10.0, float(ghost.progress), false)
		GlyphArt.draw(ci, int(ghost.g), gp, 160.0, Color(SIDE_INK[side].r, SIDE_INK[side].g, SIDE_INK[side].b, 0.85), 4.0, float(ghost.progress), false)

func _draw_glow() -> void:
	var ci := _glow
	for s in duel.spells:
		var p := Vector2(sx(s.x), LANE_Y[s.lane] + FLY)
		var dir := 1.0 if s.side == 0 else -1.0
		var w := float(s.id) * 1.7
		if s.frozen > 0:
			ci.draw_circle(p, 40, Color(0.4, 0.8, 1.0, 0.25))
		match s.kind:
			Glyphs.FIRE:
				ci.draw_circle(p, 30, Color(1.0, 0.35, 0.05, 0.35))
				ci.draw_circle(p, 18 + sin(_t * 30.0 + w) * 2.0, Color(1.0, 0.6, 0.15, 0.8))
				ci.draw_circle(p + Vector2(dir * 4, 0), 9, Color(1.0, 0.95, 0.7, 1.0))
			Duel.FIRESTORM:
				for k in 4:
					var r := 26.0 + k * 14.0
					var a0 := _t * 12.0 + k * 1.3
					ci.draw_arc(p + Vector2(0, -k * 10), r, a0, a0 + 4.2, 24, Color(1.0, 0.5 - k * 0.08, 0.1, 0.75), 7.0, true)
				ci.draw_circle(p, 22, Color(1.0, 0.85, 0.4, 0.8))
			Glyphs.WATER:
				var pts := PackedVector2Array()
				for k in 19:
					var a := -PI / 2 + PI * k / 18.0
					pts.append(p + Vector2(dir * cos(a) * 30.0, sin(a) * 52.0) + Vector2(0, sin(_t * 8.0 + k) * 3.0))
				ci.draw_polyline(pts, Color(0.2, 0.5, 1.0, 0.75), 14.0, true)
				ci.draw_polyline(pts, Color(0.75, 0.9, 1.0, 0.9), 4.0, true)
			Glyphs.WIND:
				for k in 3:
					var r := 18.0 + k * 11.0
					var a0 := -_t * (14.0 - k * 2.0) * dir + k
					ci.draw_arc(p, r, a0, a0 + 3.6, 20, Color(0.75, 1.0, 0.85, 0.7 - k * 0.15), 4.0, true)
			Glyphs.ICE:
				var dia := PackedVector2Array([p + Vector2(dir * 26, 0), p + Vector2(0, -12), p + Vector2(-dir * 26, 0), p + Vector2(0, 12)])
				ci.draw_colored_polygon(dia, Color(0.55, 0.85, 1.0, 0.75))
				ci.draw_polyline(PackedVector2Array([dia[0], dia[1], dia[2], dia[3], dia[0]]), Color(0.95, 1.0, 1.0, 0.9), 2.0, true)
			Duel.LANCE:
				ci.draw_line(p - Vector2(dir * 80, 0), p + Vector2(dir * 40, 0), Color(0.6, 0.9, 1.0, 0.5), 16.0, true)
				ci.draw_line(p - Vector2(dir * 80, 0), p + Vector2(dir * 40, 0), Color(0.95, 1.0, 1.0, 1.0), 5.0, true)
			Glyphs.STAR:
				var pts := PackedVector2Array()
				var r0 := 16.0 * (1.0 + 0.15 * sin(_t * 20.0 + w))
				for k in 10:
					var a := -PI / 2 + k * PI / 5.0 + _t * 3.0
					var r := r0 if k % 2 == 0 else r0 * 0.45
					pts.append(p + Vector2(cos(a), sin(a)) * r)
				ci.draw_colored_polygon(pts, Color(1.0, 0.92, 0.5, 0.95))
			Duel.MAGMA:
				ci.draw_circle(p + Vector2(0, 20), 44, Color(1.0, 0.25, 0.02, 0.35))
				ci.draw_circle(p + Vector2(0, 20), 26, Color(1.0, 0.5, 0.1, 0.6))
			Glyphs.BOLT, Duel.STORM:
				# Charging: a warning down the target lane, crackles at the caster.
				var y: float = LANE_Y[s.lane] + FLY
				var k := 1.0 - clampf(float(s.t) / 24.0, 0.0, 1.0)
				var c := Color(1.0, 0.95, 0.5, 0.15 + 0.35 * k * (0.5 + 0.5 * sin(_t * 40.0)))
				ci.draw_line(Vector2(X0 + 40, y), Vector2(X1 - 40, y), c, 10.0 + 10.0 * k, true)
				var o := mage_pos(s.side) + Vector2(dir * 60, 60)
				for n in 3:
					var q := o + Vector2(randf_range(-30, 30), randf_range(-60, 20))
					ci.draw_line(o, q, Color(1.0, 1.0, 0.7, 0.8), 2.0, true)
	# Mages' wards.
	for i in 2:
		var m := duel.mages[i]
		var c := mage_pos(i) + Vector2(0, 40)
		if m.shield > 0:
			var a := clampf(float(m.shield) / 2000.0, 0.25, 1.0)
			ci.draw_circle(c, 120, Color(1.0, 0.8, 0.35, 0.08 * a))
			ci.draw_arc(c, 120, 0, TAU, 64, Color(1.0, 0.85, 0.45, 0.6 * a), 4.0, true)
			ci.draw_arc(c, 112, _t, _t + 1.2, 16, Color(1.0, 1.0, 0.8, 0.5 * a), 3.0, true)
			if m.mirror:
				ci.draw_arc(c, 130, -_t, -_t + 4.0, 32, Color(0.85, 0.9, 1.0, 0.8), 3.0, true)
		if m.seal_t > 0:
			GlyphArt.draw(ci, Glyphs.SEAL, c + Vector2(0, -230), 60, Color(1.0, 0.6, 0.85, 0.85), 4.0)
		if m.time_t > 0:
			ci.draw_arc(c + Vector2(0, -230), 40, -PI / 2, -PI / 2 + TAU * m.time_t / 240.0, 32, Color(0.85, 0.7, 1.0, 0.8), 5.0, true)
