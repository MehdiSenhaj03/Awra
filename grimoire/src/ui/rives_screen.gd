class_name RivesScreen
extends Screen
## The Two Banks on screen: the river across the middle, the day bank above and the night bank below scrolling as
## the mages walk, ink creatures, obstacles with the glyph that clears them, the Beast of the River.
##
## cfg: {mode: "solo"|"local"|"net", level, boss, seed, known, chapter, host: bool}

signal finished(r: Dictionary)

const PATH_Y := [400.0, 700.0]
const RIVER_Y := 540.0
const OB_GLYPH := {Rives.O.THORNS: Glyphs.FIRE, Rives.O.FOG: Glyphs.WIND, Rives.O.CHASM: Glyphs.WALL, Rives.O.DARK: Glyphs.STAR}

var cfg: Dictionary
var rives: Rives
var mode := "solo"
var me := 0
var pens: Array[Pen] = []
var pen_player: Array[int] = []
var known: Array = []
var online: Node = null
var fx: Fx2
var _queue: Array = [[], []]
var _world: Node2D
var _glow: Node2D
var _hud: Control
var _bg: Array[TextureRect] = []
var _bg_m: Array[ShaderMaterial] = []
var _mage_tex: Array[Texture2D] = []
var _mon: Texture2D
var _t := 0.0
var _over_t := -1.0
var _sent := false
var _result: Dictionary = {}
var _foe_flash: Dictionary = {}
var _banner: Label
var _banner_sub: Label
var _remote_strokes: Array = []

func _init(c: Dictionary) -> void:
	super()
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	cfg = c
	mode = c.get("mode", "solo")
	var players := 1 if mode == "solo" else 2
	rives = Rives.new(int(c.get("seed", 3)), int(c.get("level", 0)), bool(c.get("boss", false)), players)
	known = c.get("known", range(Glyphs.COUNT))
	if c.get("skip", false):
		for b in rives.banks:
			b.walked = rives.length - 4100
		rives.waves.clear()
	# Two scrolling halves of the river painting.
	var tex := Kit.tex("rives")
	for i in 2:
		var r := TextureRect.new()
		r.texture = tex
		r.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		r.stretch_mode = TextureRect.STRETCH_SCALE
		r.position = Vector2(0, 0 if i == 0 else 540.0)
		r.size = Vector2(Kit.W, 540.0)
		r.mouse_filter = Control.MOUSE_FILTER_IGNORE
		var m := ShaderMaterial.new()
		m.shader = preload("res://src/render/shaders/scroll.gdshader")
		m.set_shader_parameter("v0", 0.0 if i == 0 else 0.5)
		m.set_shader_parameter("v1", 0.5 if i == 0 else 1.0)
		r.material = m
		add_child(r)
		_bg.append(r)
		_bg_m.append(m)
	var shade := ColorRect.new()
	shade.material = ShaderMaterial.new()
	(shade.material as ShaderMaterial).shader = preload("res://src/render/shaders/vellum.gdshader")
	shade.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(shade)
	Kit.full(shade)
	_world = Node2D.new()
	add_child(_world)
	_world.draw.connect(_draw_world)
	_glow = Node2D.new()
	var gm := CanvasItemMaterial.new()
	gm.blend_mode = CanvasItemMaterial.BLEND_MODE_ADD
	_glow.material = gm
	add_child(_glow)
	_glow.draw.connect(_draw_glow)
	fx = Fx2.new()
	add_child(fx)
	_mage_tex = [Kit.tex("fig_ysolde"), Kit.tex("fig_rival_aldric")]
	_mon = Kit.tex("monsters")
	if mode == "solo":
		_add_pen("mouse", 0)
		for dv in Input.get_connected_joypads():
			_add_pen("pad%d" % dv, 0)
			break
	elif mode == "local":
		_add_pen("mouse", 0)
		var pads := Input.get_connected_joypads()
		_add_pen("pad%d" % (pads[0] if pads.size() > 0 else 0), 1)
	else:
		me = 0 if c.get("host", true) else 1
		_add_pen("mouse", me)
	_hud = Control.new()
	Kit.full(_hud)
	_hud.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_hud)
	_hud.draw.connect(_draw_hud)
	_banner = Kit.label(self, "", 78, Kit.GOLD, "deco", 400, 0.04, Color(0.3, 0.1, 0.02, 0.8), 24)
	_banner.position = Vector2(0, RIVER_Y - 110)
	_banner.size = Vector2(Kit.W, 110)
	_banner.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_banner_sub = Kit.label(self, "", 24, Kit.IVORY, "body", 700, 0.03, Color(0, 0, 0, 0.9), 10)
	_banner_sub.position = Vector2(0, RIVER_Y + 4)
	_banner_sub.size = Vector2(Kit.W, 40)
	_banner_sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	var sub := I18n.t("rives.solo_hint") if mode == "solo" else I18n.t("rives.coop_hint")
	_show_banner(I18n.t("rives.title"), sub, 3.0)

func _add_pen(src: String, player: int) -> void:
	var p := Pen.new(src)
	var pl := player
	p.glyph.connect(func(strokes: Array, center: Vector2, size: float) -> void: _on_glyph(pl, strokes, center, size))
	pens.append(p)
	pen_player.append(player)

func _show_banner(a: String, b: String, hold: float) -> void:
	_banner.text = a
	_banner_sub.text = b
	_banner.modulate.a = 0.0
	_banner_sub.modulate.a = 0.0
	var tw := create_tween()
	tw.tween_property(_banner, "modulate:a", 1.0, 0.35)
	tw.parallel().tween_property(_banner_sub, "modulate:a", 1.0, 0.5)
	if hold > 0.0:
		tw.tween_interval(hold)
		tw.tween_property(_banner, "modulate:a", 0.0, 0.5)
		tw.parallel().tween_property(_banner_sub, "modulate:a", 0.0, 0.5)

static func sx(x: int) -> float:
	return lerpf(60.0, 1860.0, float(x) / Rives.LEN)

static func ux(px: float) -> int:
	return int(clampf((px - 60.0) / 1800.0, 0.0, 1.0) * Rives.LEN)

func bank_of(y: float) -> int:
	# The night bank's walkers stand on its near edge, their heads over the water: the line sits a little low.
	return 0 if y < RIVER_Y + 40.0 else 1

# ---- Input --------------------------------------------------------------------------------------------------------

func _on_glyph(player: int, strokes: Array, center: Vector2, size: float) -> void:
	var bank := bank_of(center.y)
	# In co-op, your hand only reaches the other bank.
	if mode != "solo" and bank == player:
		fx.text(I18n.t("rives.other_bank"), center, Color(1, 0.7, 0.5), 26)
		Sound.sfx("fizzle")
		return
	var r := Glyphs.recognize(strokes)
	var g: int = r.g
	if g >= 0 and not known.has(g):
		fx.text(I18n.t("duel.unknown"), center, Color(1, 0.6, 0.5), 26)
		g = -1
	var x := ux(center.x)
	if g < 0:
		fx.burst(center, Color(0.3, 0.2, 0.15, 0.8), 18, 300, 6)
		fx.text(I18n.t("duel.fizzle"), center, Color(1, 0.55, 0.45), 30)
		Sound.sfx("fizzle")
		return
	var sf := clampf(lerpf(0.8, 1.25, (size - 150.0) / 300.0), 0.8, 1.25)
	var qf := lerpf(0.7, 1.15, clampf((float(r.score) - Glyphs.REJECT) / (1.0 - Glyphs.REJECT), 0.0, 1.0))
	var pw := clampi(int(100.0 * sf * qf), 50, 140)
	fx.glyph(g, center, clampf(size * 0.7, 80, 240), GlyphArt.COLORS[g], 0.7)
	_queue[player].append({"g": g, "bank": bank, "x": x, "pow": pw})

func local_net_input() -> Array:
	var c: Dictionary = _queue[me].pop_front() if not _queue[me].is_empty() else {}
	if c.is_empty():
		return [0, 0]
	return [10 + (int(c.g) + 1) * 1000000 + int(c.bank) * 100000 + int(c.x) / 10 * 100 + int(c.pow) / 2, 0]

static func _decode(v: int) -> Dictionary:
	if v == 0:
		return {}
	v -= 10
	return {"g": v / 1000000 - 1, "bank": (v / 100000) % 10, "x": ((v / 100) % 1000) * 10, "pow": (v % 100) * 2}

func step_net(host_in: Array, guest_in: Array) -> void:
	rives.step([_decode(int(host_in[0])), _decode(int(guest_in[0]))])
	_events()

func step() -> void:
	var casts: Array = []
	for p in (1 if mode == "solo" else 2):
		casts.append(_queue[p].pop_front() if not _queue[p].is_empty() else {})
	rives.step(casts)
	_events()

func _events() -> void:
	for e in rives.events:
		var bank: int = e.get("bank", 0)
		var y: float = PATH_Y[bank]
		match e.t:
			"cast":
				var g: int = e.g
				var p := Vector2(sx(int(e.x)), y - 40)
				var col: Color = GlyphArt.COLORS[g]
				# The spell crosses the river from the caster's hand.
				var from := _mage_pos(1 - bank) if mode != "solo" else Vector2(Kit.W / 2.0, RIVER_Y)
				if g == Glyphs.BOLT:
					fx.lightning(from, p, Color(col.r, col.g, col.b, 0.6), 0.25, 3)
				else:
					fx.ring(p, col, 20, 220, 0.6, 6)
				fx.burst(p, col, 30, 420, 6)
				if g == Glyphs.FIRE or g == Glyphs.EARTH:
					fx.shake = 0.25
				Sound.sfx("g_%s" % Glyphs.NAMES[g])
			"combo":
				var p := Vector2(sx(int(e.x)), y - 80)
				fx.ring(p, Kit.GOLD, 30, 320, 0.9, 9)
				fx.text(I18n.t("rives.duo") if bool(e.duo) else I18n.t("rives.combo"), p + Vector2(0, -90), Kit.GOLD, 42, 1.4)
				Sound.sfx("combo")
			"foe_hit":
				_foe_flash[int(e.id)] = 0.25
				fx.burst(Vector2(sx(int(e.x)), y - 50), GlyphArt.COLORS[int(e.g)], 10, 260, 5)
			"foe_dead":
				fx.burst(Vector2(sx(int(e.x)), y - 50), Color(0.05, 0.03, 0.08, 0.9), 34, 380, 8, 0.9, 120.0)
				fx.burst(Vector2(sx(int(e.x)), y - 50), Kit.GOLD, 12, 260, 4)
				Sound.sfx("ink_die")
			"mage_hit":
				fx.burst(_mage_pos(bank) + Vector2(0, 40), Color(1.0, 0.35, 0.25), 14, 300, 6)
				Sound.sfx("hit")
			"cleared":
				fx.ring(Vector2(sx(int(e.x)), y - 60), Kit.GOLD, 30, 260, 0.8, 6)
				fx.text(I18n.t("rives.cleared"), Vector2(sx(int(e.x)), y - 170), Kit.GOLD, 32)
				Sound.sfx("clear")
			"wall_break":
				fx.burst(Vector2(sx(int(e.x)), y - 40), Color(0.85, 0.75, 0.55), 30, 420, 7)
				Sound.sfx("break")
			"bolt":
				fx.lightning(Vector2(sx(int(e.x)), y - 520), Vector2(sx(int(e.x)), y - 40), Color(1.0, 0.95, 0.6), 0.35, 6)
				Sound.sfx("thunder")
			"beast_rise":
				_show_banner(I18n.t("rives.beast"), I18n.t("rives.beast_sub"), 2.8)
				fx.shake = 1.0
				Sound.sfx("beast")
			"beast_hit":
				fx.burst(Vector2(1500, RIVER_Y + (-120 if bank == 0 else 120)), Color(1.0, 0.85, 0.3), 26, 420, 7)
				fx.shake = 0.3
			"strike_warn":
				pass
			"strike":
				var p := Vector2(sx(int(e.x)), y - 30)
				fx.burst(p, Color(0.05, 0.03, 0.08, 0.9), 30, 460, 9, 0.8, 200.0)
				fx.shake = 0.6
				Sound.sfx("hit")
			"dispel":
				fx.glyph(Glyphs.SEAL, Vector2(sx(int(e.x)), y - 100), 140, GlyphArt.COLORS[Glyphs.SEAL], 0.9)
				Sound.sfx("seal")
			"noink":
				fx.text(I18n.t("duel.noink"), Vector2(Kit.W / 2.0, y - 200), Color(0.5, 0.75, 1.0), 30)
			"over":
				_over_t = 0.0
				var won: bool = e.won
				_show_banner(I18n.t("rives.won") if won else I18n.t("rives.lost"), I18n.t("rives.won_sub") if won else I18n.t("rives.lost_sub"), 0.0)
				Sound.sfx("win" if won else "lose")
				_result = {"mode": "rives", "won": won, "frame": rives.frame, "chapter": cfg.get("chapter", 0)}

func _mage_pos(bank: int) -> Vector2:
	return Vector2(sx(Rives.MAGE_X) - 30.0, PATH_Y[bank] - 120.0)

# ---- Frame ------------------------------------------------------------------------------------------------------------

func frame(dt: float) -> void:
	_t += dt
	var mp := get_viewport().get_mouse_position()
	for p in pens:
		p.enabled = rives.over == ""
		p.update(dt, mp)
	for i in 2:
		var b: Rives.Bank = rives.banks[i]
		_bg_m[i].set_shader_parameter("offset", float(b.walked) / 30000.0)
	for id: int in _foe_flash.keys():
		_foe_flash[id] = float(_foe_flash[id]) - dt
		if float(_foe_flash[id]) <= 0.0:
			_foe_flash.erase(id)
	if online and mode == "net":
		var pts: Array = []
		for k in pens.size():
			for s: PackedVector2Array in pens[k].strokes:
				pts.append(s)
		online.call("send_strokes", pts)
	var danger := 0.0
	for f in rives.foes:
		danger = maxf(danger, 1.0 - clampf((f.x - Rives.MAGE_X) / 5000.0, 0.0, 1.0))
	Sound.tension(maxf(danger * 0.8, 1.0 if rives.beast_t > 0 and rives.beast_hp > 0 else 0.0))
	_world.queue_redraw()
	_glow.queue_redraw()
	_hud.queue_redraw()
	position = Vector2(randf_range(-1, 1), randf_range(-1, 1)) * fx.shake * 12.0
	if _over_t >= 0.0:
		_over_t += dt
		if _over_t > 3.4 and not _sent:
			_sent = true
			finished.emit(_result)

func remote_strokes(pts: Array) -> void:
	_remote_strokes = pts

func _draw_world() -> void:
	var ci := _world
	# Obstacles, with the glyph that clears them.
	for o in rives.obstacles:
		if o.cleared:
			continue
		var x := rives.obstacle_x(o)
		if x < 0:
			continue
		var p := Vector2(sx(x), PATH_Y[o.bank])
		match o.kind:
			Rives.O.THORNS:
				for k in 9:
					var a := k * 0.7
					var base := p + Vector2(-90 + k * 22, 20)
					ci.draw_polyline(PackedVector2Array([base, base + Vector2(cos(a) * 30, -60 - sin(a * 3.0) * 30), base + Vector2(sin(a) * 50, -120)]), Color(0.12, 0.08, 0.05), 6.0, true)
					ci.draw_circle(base + Vector2(sin(a) * 50, -120), 5, Color(0.5, 0.05, 0.08))
			Rives.O.FOG:
				for k in 7:
					var c := p + Vector2(-140 + k * 46 + sin(_t + k) * 10, -60 - (k % 3) * 40)
					ci.draw_circle(c, 70, Color(0.85, 0.88, 0.9, 0.22))
			Rives.O.CHASM:
				ci.draw_rect(Rect2(p + Vector2(-110, -10), Vector2(220, 60)), Color(0.02, 0.01, 0.02, 0.95))
				ci.draw_rect(Rect2(p + Vector2(-110, -10), Vector2(220, 60)), Color(0.4, 0.3, 0.2), false, 3.0)
			Rives.O.DARK:
				for k in 6:
					ci.draw_circle(p + Vector2(-100 + k * 40, -90 + sin(_t * 2.0 + k) * 20), 80, Color(0.02, 0.01, 0.04, 0.55))
		var gl: int = OB_GLYPH[o.kind]
		var gp := p + Vector2(0, -230)
		ci.draw_circle(gp, 46, Color(0.08, 0.04, 0.02, 0.75))
		ci.draw_arc(gp, 46, 0, TAU, 32, Kit.GOLD, 2.0, true)
		GlyphArt.draw(ci, gl, gp, 52, GlyphArt.COLORS[gl], 3.0, 0.5 + 0.5 * fmod(_t * 0.6, 1.0) if known.has(gl) else 1.0)
	# Walls.
	for bi in 2:
		var b: Rives.Bank = rives.banks[bi]
		if b.wall_hp > 0:
			var r := Rect2(Vector2(sx(b.wall_x) - 24, PATH_Y[bi] - 150), Vector2(48, 170))
			ci.draw_rect(r, Color(0.86, 0.78, 0.6))
			ci.draw_rect(r, Kit.GOLD, false, 4.0)
	# Telegraphed strikes.
	for s: Dictionary in rives.strikes:
		var p := Vector2(sx(int(s.x)), PATH_Y[int(s.bank)])
		var k := 1.0 - float(s.t) / 90.0
		ci.draw_circle(p, 110, Color(0.8, 0.1, 0.1, 0.12 + 0.25 * k))
		ci.draw_arc(p, 110 * (1.0 - k) + 10, 0, TAU, 40, Color(1.0, 0.3, 0.2, 0.8), 4.0, true)
	# The Beast: a giant of living ink rising from the river (the blot creature, huge), tentacles reaching for the
	# banks, its golden eye turned to one of them.
	if rives.boss and rives.beast_t > 0 and rives.beast_hp > 0:
		var rise := clampf(rives.beast_t / 240.0, 0.0, 1.0)
		var c := Vector2(1450, RIVER_Y + 60.0 + 260.0 * (1.0 - rise))
		var eye_up := rives.beast_eye == 0
		for k in 6:
			var a0 := (-0.75 if k % 2 == 0 else 0.75) + sin(_t * 1.1 + k) * 0.2
			var pts := PackedVector2Array()
			for j in 24:
				var tt := j / 23.0
				var reach := 330.0 + k * 45.0
				var curl := sin(tt * PI * 1.6 + _t * 2.2 + k * 1.7) * 70.0 * tt
				pts.append(c + Vector2(-reach * tt, sin(a0) * 280.0 * tt + curl) * rise)
			var half := pts.slice(0, 14)
			var tip := pts.slice(12)
			ci.draw_polyline(half, Color(0.07, 0.05, 0.14, 0.92), 34.0 * rise, true)
			ci.draw_polyline(tip, Color(0.07, 0.05, 0.14, 0.92), 14.0 * rise, true)
			ci.draw_polyline(pts, Color(0.75, 0.6, 0.3, 0.35), 2.0, true)
		if _mon:
			var cw := _mon.get_width() / 3.0
			var chh := _mon.get_height() / 2.0
			var size := 640.0 * rise
			var wob := 1.0 + 0.04 * sin(_t * 2.0)
			ci.draw_texture_rect_region(_mon, Rect2(c - Vector2(size * wob, size / wob) / 2.0, Vector2(size * wob, size / wob)), Rect2(1 * cw, 1 * chh, cw, chh), Color(0.7, 0.7, 0.9))
		var eye := c + Vector2(-60, -150 if eye_up else 120) * rise
		ci.draw_circle(eye, 70 * rise, Color(1.0, 0.85, 0.3, 0.25))
		ci.draw_circle(eye, 44 * rise, Color(0.98, 0.78, 0.25))
		ci.draw_circle(eye + Vector2(-10, 0) * rise, 18 * rise, Color(0.05, 0.02, 0.0))
	# Foes.
	for f in rives.foes:
		var p := Vector2(sx(f.x), PATH_Y[f.bank] - (110.0 if Rives.ESPEC[f.kind][3] else 60.0) + sin(_t * 6.0 + f.id) * 6.0)
		_draw_foe(ci, f, p)
	# Mages.
	for bi in 2:
		var tex := _mage_tex[bi]
		var p := _mage_pos(bi)
		var b: Rives.Bank = rives.banks[bi]
		var bob := sin(_t * 8.0) * 4.0 if not b.blocked else 0.0
		if tex:
			var h := 230.0
			var w := h * tex.get_width() / tex.get_height()
			var mod := Color(1, 1, 1) if b.hit_t > 8 or int(_t * 12.0) % 2 == 0 else Color(1.7, 0.7, 0.7)
			if bi == 1:
				# Aldric's figure faces left: mirror it so both walk right.
				ci.draw_set_transform(p + Vector2(0, bob), 0.0, Vector2(-1, 1))
				ci.draw_texture_rect(tex, Rect2(Vector2(-w / 2, -h / 2), Vector2(w, h)), false, mod)
				ci.draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE)
			else:
				ci.draw_texture_rect(tex, Rect2(p + Vector2(-w / 2, -h / 2 + bob), Vector2(w, h)), false, mod)

func _draw_foe(ci: Node2D, f: Rives.Foe, p: Vector2) -> void:
	var size := 150.0 if f.kind != Rives.E.GOLEM else 210.0
	var flash: bool = _foe_flash.has(f.id)
	var mod := Color(1, 1, 1)
	if f.frozen > 0:
		mod = Color(0.6, 0.85, 1.3)
	if flash:
		mod = Color(2.0, 1.6, 1.2)
	if _mon:
		var cw := _mon.get_width() / 3.0
		var chh := _mon.get_height() / 2.0
		var src := Rect2((f.kind % 3) * cw, (f.kind / 3) * chh, cw, chh)
		ci.draw_texture_rect_region(_mon, Rect2(p - Vector2(size, size) / 2.0, Vector2(size, size)), src, mod)
	else:
		ci.draw_circle(p, size * 0.3, Color(0.05, 0.03, 0.06))
	# Health.
	var k := clampf(float(f.hp) / f.max_hp, 0.0, 1.0)
	if k < 1.0:
		ci.draw_rect(Rect2(p + Vector2(-40, -size * 0.5 - 12), Vector2(80, 6)), Color(0, 0, 0, 0.6))
		ci.draw_rect(Rect2(p + Vector2(-40, -size * 0.5 - 12), Vector2(80 * k, 6)), Color(0.9, 0.75, 0.3))

func _draw_glow() -> void:
	var ci := _glow
	for bi in 2:
		var b: Rives.Bank = rives.banks[bi]
		var c := _mage_pos(bi) + Vector2(0, 20)
		if b.shield > 0:
			ci.draw_arc(c, 100, 0, TAU, 48, Color(1.0, 0.85, 0.45, 0.6), 4.0, true)
			ci.draw_circle(c, 100, Color(1.0, 0.8, 0.35, 0.06))
		if b.slow_t > 0:
			ci.draw_line(Vector2(sx(Rives.MAGE_X + 400), PATH_Y[bi] + 20), Vector2(1860, PATH_Y[bi] + 20), Color(0.8, 0.65, 1.0, 0.18), 30.0)

func _draw_hud() -> void:
	var ci := _hud
	var font := Kit.font("display", 700, 0.08, 20)
	for bi in 2:
		var b: Rives.Bank = rives.banks[bi]
		var y := 34.0 if bi == 0 else Kit.H - 76.0
		_bar(ci, Rect2(40, y, 360, 18), float(b.hp) / Rives.HP, Color("#c8322a"))
		_bar(ci, Rect2(40, y + 26, 300, 10), float(b.ink) / Rives.INK, Color("#2a5fc8"))
		var name := I18n.t("names.ysolde") if bi == 0 else I18n.t("names.aldric")
		ci.draw_string(font, Vector2(420, y + 18), name, HORIZONTAL_ALIGNMENT_LEFT, -1, 22, Kit.IVORY)
		# Road travelled.
		var k := clampf(float(b.walked) / rives.length, 0.0, 1.0)
		ci.draw_rect(Rect2(Kit.W - 460, y + 6, 400, 6), Color(0, 0, 0, 0.5))
		ci.draw_rect(Rect2(Kit.W - 460, y + 6, 400 * k, 6), Kit.GOLD)
		ci.draw_circle(Vector2(Kit.W - 460 + 400 * k, y + 9), 9, Kit.GOLD)
		if b.blocked and rives.over == "":
			ci.draw_string(font, Vector2(Kit.W - 460, y + 40), I18n.t("rives.blocked"), HORIZONTAL_ALIGNMENT_LEFT, -1, 18, Color(1, 0.6, 0.45))
	if rives.boss and rives.beast_t > 0:
		var k := clampf(float(rives.beast_hp) / maxf(1.0, rives.beast_max), 0.0, 1.0)
		var r := Rect2(Kit.W / 2.0 - 400, 120, 800, 16)
		ci.draw_rect(r.grow(3), Color(0.05, 0.02, 0.05, 0.9))
		ci.draw_rect(Rect2(r.position, Vector2(r.size.x * k, r.size.y)), Color(0.55, 0.25, 0.75))
		ci.draw_rect(r.grow(3), Kit.GOLD, false, 2.0)
	# Ink of the pens.
	for k in pens.size():
		var col := Color("#ffd27a") if pen_player[k] == 0 else Color("#9fd0ff")
		GlyphArt.ink(ci, pens[k].strokes, col, 8.0)
		if pens[k].source != "mouse":
			ci.draw_circle(pens[k].cursor, 9, col)
	if not _remote_strokes.is_empty():
		GlyphArt.ink(ci, _remote_strokes, Color("#9fd0ff") if me == 0 else Color("#ffd27a"), 8.0)

func _bar(ci: Control, r: Rect2, k: float, a: Color) -> void:
	ci.draw_rect(r.grow(3), Color(0.08, 0.04, 0.02, 0.85))
	ci.draw_rect(Rect2(r.position, Vector2(r.size.x * clampf(k, 0.0, 1.0), r.size.y)), a)
	ci.draw_rect(r.grow(3), Color(0.9, 0.7, 0.35, 0.85), false, 2.0)
