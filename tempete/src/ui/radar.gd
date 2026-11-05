class_name Radar
extends Control
## The truck's Doppler radar on the whole map: reflectivity (rain, hail, the hook that announces the tornado) or
## radial velocity (red away, green toward: the couplet is the rotation), refreshed by a sweep like the real thing.
## Roads, river, towns, the vehicles, probes, Eli's route and the waypoint on top. Click to set the waypoint.

signal waypoint(p: Vector2i)

const RES := 96
const PAD := 0.06          # map margin around the 12 km (fraction)
var chase: Chase
var team := 0
var velocity := false
var route: Array[Vector2i] = []
var waypoint_pos := Vector2i(-1, -1)
var clickable := true
var _img: Image
var _tex: ImageTexture
var _row := 0
var _t := 0.0
var _font: Font

func _init(c: Chase) -> void:
	chase = c
	_img = Image.create(RES, RES, false, Image.FORMAT_RGBA8)
	_img.fill(Color(0, 0, 0, 0))
	_tex = ImageTexture.create_from_image(_img)
	mouse_filter = Control.MOUSE_FILTER_STOP
	_font = Kit.font("mono", 600, 0.0, 13)

func _to_px(x: int, y: int) -> Vector2:
	var s := size.x / (1.0 + 2.0 * PAD)
	return Vector2((x / float(Plains.SIZE) + PAD) * s, (1.0 - y / float(Plains.SIZE) + PAD) * s)

func _to_map(p: Vector2) -> Vector2i:
	var s := size.x / (1.0 + 2.0 * PAD)
	return Vector2i(int((p.x / s - PAD) * Plains.SIZE), int((1.0 - (p.y / s - PAD)) * Plains.SIZE))

func _gui_input(e: InputEvent) -> void:
	if clickable and e is InputEventMouseButton and e.pressed and e.button_index == MOUSE_BUTTON_LEFT:
		var m := _to_map(e.position)
		waypoint.emit(Vector2i(clampi(m.x, 0, Plains.SIZE), clampi(m.y, 0, Plains.SIZE)))
		accept_event()

## Reflectivity 0..100 at a point, with the hook echo wrapped around each rotating storm's mesocyclone.
func dbz(x: int, y: int) -> int:
	var r := chase.rain_at(x, y)
	var h := chase.hail_at(x, y)
	for st in chase.storms:
		if st.dead or st.rot() < 450:
			continue
		var hk := chase.hook(st)
		var d := Trig.rot(x - hk.x, y - hk.y, -st.motion())
		var dist := Trig.isqrt((d.x / 1000) * (d.x / 1000) + (d.y / 1000) * (d.y / 1000))
		# A crescent on the right-rear side of the hook point.
		if dist > 300 and dist < 1300 and d.y < 300_000:
			var ang := Trig.atan2i(d.y, d.x)
			if ang > 1300 and ang < 3500:
				r = maxi(r, 55 * st.rot() / 1000 * (1300 - absi(dist - 800)) / 1300 + 20)
	return mini(100, r * 85 / 100 + h / 3)

func _process(dt: float) -> void:
	_t += dt
	# Sweep a few rows per frame.
	var v: Chase.Vehicle = chase.vehicles[team]
	for k in 6:
		var j := _row
		for i in RES:
			var x := int(((i + 0.5) / RES * (1.0 + 2.0 * PAD) - PAD) * Plains.SIZE)
			var y := int((1.0 - ((j + 0.5) / RES * (1.0 + 2.0 * PAD) - PAD)) * Plains.SIZE)
			var c := Color(0, 0, 0, 0)
			if velocity:
				var z := dbz(x, y)
				if z > 12:
					var w := chase.wind_at(x, y)
					var ux := x - v.x
					var uy := y - v.y
					var d := maxf(1.0, Vector2(ux, uy).length())
					var radial := (w.x * ux + w.y * uy) / d
					c = Color(0.95, 0.2, 0.15, 0.9).lerp(Color(1, 1, 1, 0.9), 0.0) if radial > 0 else Color(0.2, 0.9, 0.3, 0.9)
					c.a = clampf(absf(radial) / 25.0, 0.2, 0.95)
			else:
				c = _color(dbz(x, y))
			_img.set_pixel(i, j, c)
		_row = (_row + 1) % RES
	_tex.update(_img)
	queue_redraw()

static func _color(z: int) -> Color:
	# The usual reflectivity ramp: light greens in light rain, yellow, orange, red in the core, magenta for hail.
	if z < 10:
		return Color(0, 0, 0, 0)
	if z < 25:
		return Color(0.3, 0.8, 0.4, 0.45)
	if z < 40:
		return Color(0.1, 0.6, 0.18, 0.75)
	if z < 55:
		return Color(0.95, 0.88, 0.2, 0.85)
	if z < 70:
		return Color(0.98, 0.55, 0.1, 0.9)
	if z < 88:
		return Color(0.86, 0.1, 0.1, 0.95)
	return Color(0.85, 0.2, 0.9, 0.95)

func _draw() -> void:
	var r := Rect2(Vector2.ZERO, size)
	draw_rect(r, Color(0.02, 0.04, 0.05, 0.92))
	# Roads and river.
	var pl := chase.plains
	for k in range(1, Plains.N):
		for j in Plains.N:
			for vert in [true, false]:
				var t: int = int(pl.vroad[k][j]) if vert else int(pl.hroad[k][j])
				if t == 0:
					continue
				var a := _to_px(k * Plains.CELL, j * Plains.CELL) if vert else _to_px(j * Plains.CELL, k * Plains.CELL)
				var b := _to_px(k * Plains.CELL, (j + 1) * Plains.CELL) if vert else _to_px((j + 1) * Plains.CELL, k * Plains.CELL)
				draw_line(a, b, Color(0.8, 0.8, 0.8, 0.35) if t == 2 else Color(0.6, 0.5, 0.35, 0.3), 2.0 if t == 2 else 1.0)
	var rv := pl.river
	var pts := PackedVector2Array()
	for i in range(0, rv.size(), 2):
		pts.append(_to_px(rv[i], rv[i + 1]))
	draw_polyline(pts, Color(0.3, 0.5, 0.8, 0.6), 2.0)
	for t: Dictionary in pl.towns:
		draw_circle(_to_px(int(t.x), int(t.y)), 5, Color(0.9, 0.9, 0.9, 0.5))
	# Echoes.
	draw_texture_rect(_tex, r, false)
	# Sweep line.
	var c := size / 2.0
	var ang := fmod(_t * 1.6, TAU)
	draw_line(c, c + Vector2(cos(ang), sin(ang)) * size.x * 0.75, Color(0.4, 1.0, 0.6, 0.18), 2.0)
	# Tornado warnings: the track ahead of each tornado.
	for tn in chase.tornadoes:
		if tn.dead:
			continue
		var st := chase._storm(tn.storm)
		if st == null:
			continue
		var p0 := _to_px(tn.x, tn.y)
		var ahead := Trig.rot(4_000_000, 0, st.motion())
		var p1 := _to_px(tn.x + ahead.x, tn.y + ahead.y)
		var side := (p1 - p0).orthogonal().normalized() * 14.0
		draw_colored_polygon(PackedVector2Array([p0 - side, p1 - side * 2.2, p1 + side * 2.2, p0 + side]), Color(1.0, 0.1, 0.1, 0.18))
		draw_polyline(PackedVector2Array([p0 - side, p1 - side * 2.2, p1 + side * 2.2, p0 + side, p0 - side]), Color(1.0, 0.2, 0.2, 0.9), 2.0)
		var blink := 0.5 + 0.5 * sin(_t * 10.0)
		draw_circle(p0, 6 + 2 * blink, Color(1, 0.15, 0.15, 0.9))
		draw_string(_font, p0 + Vector2(10, -8), "EF%d" % tn.ef, HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color(1, 0.5, 0.5))
	# Storm labels.
	for st in chase.storms:
		if st.dead:
			continue
		var p := _to_px(st.x, st.y)
		draw_arc(p, 9, 0, TAU, 16, Color(1, 1, 1, 0.5), 1.5)
		draw_string(_font, p + Vector2(11, 4), "C%d" % st.id, HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color(1, 1, 1, 0.75))
	# Route and waypoint.
	if route.size() > 1:
		var rp := PackedVector2Array()
		rp.append(_to_px(chase.vehicles[team].x, chase.vehicles[team].y))
		for p in route:
			rp.append(_to_px(p.x, p.y))
		draw_polyline(rp, Color(1.0, 0.85, 0.3, 0.75), 2.0)
	if waypoint_pos.x >= 0:
		var wp := _to_px(waypoint_pos.x, waypoint_pos.y)
		draw_line(wp + Vector2(-7, -7), wp + Vector2(7, 7), Color(0.3, 0.9, 1.0), 2.5)
		draw_line(wp + Vector2(-7, 7), wp + Vector2(7, -7), Color(0.3, 0.9, 1.0), 2.5)
	# Probes.
	for p in chase.probes:
		var pp := _to_px(p.x, p.y)
		draw_circle(pp, 4, Color(0.9, 0.9, 0.9) if not p.hit else Color(0.3, 1.0, 0.4))
	# Vehicles.
	for v in chase.vehicles:
		if not v.alive:
			continue
		var p := _to_px(v.x, v.y)
		var a := -float(v.a) / Trig.TURN * TAU
		var col := Color(1.0, 0.6, 0.15) if v.team == team else Color(0.9, 0.3, 0.9)
		var tri := PackedVector2Array([p + Vector2(cos(a), sin(a)) * 11.0, p + Vector2(cos(a + 2.5), sin(a + 2.5)) * 7.0, p + Vector2(cos(a - 2.5), sin(a - 2.5)) * 7.0])
		draw_colored_polygon(tri, col)
	draw_rect(r, Color(1.0, 0.7, 0.3, 0.5), false, 2.0)
	draw_string(_font, Vector2(10, 20), I18n.t("radar.vel") if velocity else I18n.t("radar.refl"), HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color(1, 0.8, 0.5, 0.8))
