class_name Hud
extends Control
## In-game heads-up display, and the score/judgement popups projected from the arena.

const RANK_COLORS := [Color("#7d8aa8"), Color("#7fb8ff"), Color("#2ef2ff"), Color("#40ffb0"), Color("#ffc34a"), Color("#ff7a2f"), Color("#ff2fb9")]
const POP_STYLE := {
	"perfect": [Color("#fff3c8"), 18, "display", 700, 0.18],
	"great": [Color("#9ff8ff"), 13, "display", 700, 0.15],
	"score": [Color("#cfe8ff"), 13, "mono", 700, 0.0],
	"big": [Color("#ffc34a"), 20, "mono", 700, 0.0],
	"parry": [Color("#ffffff"), 20, "display", 700, 0.2],
	"heal": [Color("#40ffb0"), 22, "display", 700, 0.0],
}

var _hp: HBoxContainer
var _hp_n := -1
var _hp_max := -1
var _gauge: ColorRect
var _gauge_fill: ColorRect
var _gauge_l: Label
var _score: Label
var _mult: Label
var _rank: Label
var _rank_name: Label
var _rank_bar: ColorRect
var _rank_box: Control
var _wave: Label
var _boss: Control
var _boss_name: Label
var _boss_fill: ColorRect
var _boss_lag: ColorRect
var _center: Control
var _hint: RichTextLabel
var _hurt: TextureRect
var _shown := 0.0
var _last_rank := -1
var _hint_t := 0
var _lag := 1.0
var _lag_wait := 0.0
var _cache: Dictionary = {}
var _pops: Array[Dictionary] = []
var _pool: Array[Label] = []
var _t := 0.0

func _init() -> void:
	Kit.full(self)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	# Low-health vignette.
	_hurt = TextureRect.new()
	var g := GradientTexture2D.new()
	var gr := Gradient.new()
	gr.offsets = PackedFloat32Array([0.0, 0.62, 1.0])
	gr.colors = PackedColorArray([Color(1, 0.06, 0.25, 0), Color(1, 0.06, 0.25, 0), Color(1, 0.06, 0.25, 0.55)])
	g.gradient = gr
	g.fill = GradientTexture2D.FILL_RADIAL
	g.fill_from = Vector2(0.5, 0.5)
	g.fill_to = Vector2(1.05, 1.05)
	_hurt.texture = g
	_hurt.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_hurt.stretch_mode = TextureRect.STRETCH_SCALE
	_hurt.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_hurt.modulate.a = 0.0
	add_child(_hurt)
	Kit.full(_hurt)
	# Top left: resonance (hp), Overdrive gauge.
	var tl := Kit.vbox(self, 10)
	tl.position = Vector2(36, 28)
	_hp = Kit.hbox(tl, 8)
	_hp.custom_minimum_size.y = 26
	_gauge = Kit.rect(tl, Color(1, 1, 1, 0.08))
	_gauge.custom_minimum_size = Vector2(210, 6)
	_gauge.clip_contents = true
	_gauge_fill = ColorRect.new()
	var gm := ShaderMaterial.new()
	gm.shader = preload("res://src/ui/shaders/gauge.gdshader")
	_gauge_fill.material = gm
	_gauge_fill.size = Vector2(0, 6)
	_gauge_fill.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_gauge.add_child(_gauge_fill)
	_gauge_l = Kit.label(tl, "", 11, Kit.DIM, "display", 500, 0.25)
	# Top right: score, multiplier.
	_score = Kit.label(self, "0", 34, Kit.PALE, "mono", 700, 0.02, Color(0.18, 0.95, 1.0, 0.33), 18)
	_score.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_score.position = Vector2(Kit.W - 40 - 500, 20)
	_score.size = Vector2(500, 44)
	_mult = Kit.label(self, "×1", 14, Kit.DIM, "display", 500, 0.2)
	_mult.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_mult.position = Vector2(Kit.W - 40 - 300, 66)
	_mult.size = Vector2(300, 20)
	# Style rank.
	_rank_box = Control.new()
	_rank_box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_rank_box.position = Vector2(Kit.W - 44 - 300, 104)
	_rank_box.size = Vector2(300, 140)
	add_child(_rank_box)
	_rank = Kit.label(_rank_box, "D", 74, Color.WHITE, "display", 800, 0.0, Color(0.18, 0.95, 1.0, 0.8), 18)
	var italic := FontVariation.new()
	italic.base_font = _rank.label_settings.font
	italic.variation_transform = Transform2D(Vector2(1, 0), Vector2(-0.2, 1), Vector2.ZERO)
	_rank.label_settings.font = italic
	_rank.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_rank.size = Vector2(300, 80)
	_rank.pivot_offset = Vector2(260, 40)
	var rm := ShaderMaterial.new()
	rm.shader = preload("res://src/ui/shaders/gradient_text.gdshader")
	rm.set_shader_parameter("vertical", 1.0)
	rm.set_shader_parameter("size", Vector2(300, 90))
	_rank.material = rm
	_rank_name = Kit.label(_rank_box, "", 11, Kit.CYAN, "display", 500, 0.4)
	_rank_name.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_rank_name.position = Vector2(0, 84)
	_rank_name.size = Vector2(300, 16)
	Kit.rect(_rank_box, Color(1, 1, 1, 0.094), Vector2(150, 110), Vector2(150, 3))
	_rank_bar = Kit.rect(_rank_box, Kit.CYAN, Vector2(150, 110), Vector2(0, 3))
	# Wave counter.
	_wave = Kit.label(self, "", 12, Kit.DIM, "display", 500, 0.5)
	_wave.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_wave.position = Vector2(0, 26)
	_wave.size = Vector2(Kit.W, 20)
	# Boss bar.
	_boss = Control.new()
	_boss.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_boss.position = Vector2((Kit.W - 820) / 2, Kit.H - 46 - 44)
	_boss.size = Vector2(820, 44)
	_boss.modulate.a = 0.0
	add_child(_boss)
	_boss_name = Kit.label(_boss, "", 15, Color.WHITE, "display", 500, 0.6, Kit.MAGENTA, 14)
	_boss_name.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_boss_name.size = Vector2(820, 24)
	Kit.rect(_boss, Color(1, 1, 1, 0.07), Vector2(0, 34), Vector2(820, 8))
	_boss_lag = Kit.rect(_boss, Color(1, 1, 1, 0.38), Vector2(0, 34), Vector2(820, 8))
	_boss_fill = ColorRect.new()
	var bm := ShaderMaterial.new()
	bm.shader = preload("res://src/ui/shaders/bossbar.gdshader")
	_boss_fill.material = bm
	_boss_fill.position = Vector2(0, 34)
	_boss_fill.size = Vector2(820, 8)
	_boss_fill.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_boss.add_child(_boss_fill)
	# Banner, hint.
	_center = Control.new()
	_center.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_center.size = Vector2(Kit.W, 200)
	_center.position = Vector2(0, Kit.H * 0.4 - 100)
	add_child(_center)
	_hint = Kit.rich(self, "", 18, Kit.PALE)
	_hint.position = Vector2(Kit.W * 0.15, Kit.H - 120 - 60)
	_hint.size = Vector2(Kit.W * 0.7, 60)
	_hint.fit_content = false
	_hint.add_theme_constant_override("shadow_offset_y", 2)
	_hint.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 0.9))
	_hint.modulate.a = 0.0

func banner(text: String, sub: String = "") -> void:
	for c in _center.get_children():
		c.queue_free()
	var box := Control.new()
	box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	box.size = _center.size
	box.pivot_offset = box.size / 2
	_center.add_child(box)
	var l := Kit.label(box, text, 64, Color.WHITE, "display", 800, 0.3, Color(0.18, 0.95, 1.0, 0.8), 30)
	l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	l.size = Vector2(Kit.W, 90)
	l.position.y = 40 if sub != "" else 60
	if sub != "":
		var s := Kit.label(box, sub, 14, Kit.CYAN, "display", 500, 0.6)
		s.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		s.size = Vector2(Kit.W, 20)
		s.position.y = 136
	box.modulate.a = 0.0
	box.scale = Vector2(1.3, 1.3)
	var tw := box.create_tween()
	tw.set_parallel()
	tw.tween_property(box, "modulate:a", 1.0, 0.26)
	tw.tween_property(box, "scale", Vector2.ONE, 0.26).set_ease(Tween.EASE_OUT)
	tw.chain().tween_interval(1.5)
	tw.chain().tween_property(box, "modulate:a", 0.0, 0.44)
	tw.parallel().tween_property(box, "scale", Vector2(0.97, 0.97), 0.44)
	tw.chain().tween_callback(box.queue_free)

func show_hint(text: String, frames: int = 360) -> void:
	_hint.text = "[center]%s[/center]" % Kit.html(text)
	_hint_t = frames

func _put(key: String, v: Variant) -> bool:
	if _cache.get(key) == v:
		return false
	_cache[key] = v
	return true

func update_hud(w: World, run: StageRunner, dt: float) -> void:
	_t += dt
	var p := w.player
	if p.hp != _hp_n or p.max_hp != _hp_max:
		for c in _hp.get_children():
			c.queue_free()
		for i in p.max_hp:
			var d := Diamond.new(i < p.hp)
			_hp.add_child(d)
		_hp_n = p.hp
		_hp_max = p.max_hp
	_hurt.modulate.a = (0.35 + sin(_t * 1000.0 / 180.0) * 0.15) if p.hp == 1 else move_toward(_hurt.modulate.a, 0.0, dt * 3.0)
	_gauge_fill.size.x = 210.0 * clampf(w.gauge / 100.0, 0.0, 1.0)
	var full := w.gauge >= 100.0
	var od := p.is_overdrive()
	(_gauge_fill.material as ShaderMaterial).set_shader_parameter("pulse", 1.0 if (full or od) else 0.0)
	var gl := I18n.t("hud.overdriveOn") if od else (I18n.t("hud.overdriveReady", {"key": Keys.key_name("overdrive")}) if full else I18n.t("hud.overdrive"))
	if _put("gl", gl):
		_gauge_l.text = gl
		_gauge_l.label_settings.font_color = Kit.GOLD if (full or od) else Kit.DIM
		_gauge_l.label_settings.shadow_color = Color(1, 0.76, 0.29, 0.7) if (full or od) else Color(0, 0, 0, 0)
		_gauge_l.label_settings.shadow_size = 10 if (full or od) else 0
	_shown += (w.score - _shown) * 0.2
	if absf(w.score - _shown) < 1.0:
		_shown = w.score
	var sc := Kit.fmt_int(_shown)
	if _put("score", sc):
		_score.text = sc
	var mt := "×%s" % str(w.mult()).trim_suffix(".0")
	if _put("mult", mt):
		_mult.text = mt
	var r := w.rank
	if r != _last_rank:
		var rc: Color = RANK_COLORS[r]
		(_rank.material as ShaderMaterial).set_shader_parameter("c1", rc)
		_rank.label_settings.shadow_color = Color(rc.r, rc.g, rc.b, 0.8)
		_rank.text = World.RANKS[r]
		_rank_name.text = I18n.t("rank.%d" % r)
		_rank_name.label_settings.font_color = rc
		_rank_bar.color = rc
		if r > _last_rank and _last_rank >= 0:
			_rank.scale = Vector2(1.7, 1.7)
			_rank.rotation = deg_to_rad(-6)
			var tw := _rank.create_tween().set_parallel()
			tw.tween_property(_rank, "scale", Vector2.ONE, 0.35).set_ease(Tween.EASE_OUT)
			tw.tween_property(_rank, "rotation", 0.0, 0.35).set_ease(Tween.EASE_OUT)
		_last_rank = r
	var lo: float = World.RANK_AT[r]
	var hi: float = World.RANK_AT[r + 1] if r + 1 < World.RANK_AT.size() else 1000.0
	_rank_bar.size.x = 150.0 * clampf((w.style - lo) / (hi - lo), 0.0, 1.0)
	_rank_bar.position.x = 300.0 - _rank_bar.size.x
	if run:
		var wt := "" if run.boss else "%s %d / %d" % [I18n.t("hud.wave"), run.wave_number, run.total - (1 if run.def.get("boss") else 0)]
		if _put("wave", wt):
			_wave.text = wt
	var boss: Enemy = run.boss if run else null
	var on := boss != null and not boss.dead
	_boss.modulate.a = move_toward(_boss.modulate.a, 1.0 if on else 0.0, dt / 0.5)
	if boss:
		var bn := I18n.t("boss.%s.name" % run.def.boss)
		if _put("bn", bn):
			_boss_name.text = bn
		var k := clampf(boss.hp / boss.max_hp, 0.0, 1.0)
		_boss_fill.size.x = lerpf(_boss_fill.size.x, 820.0 * k, 1.0 - exp(-dt / 0.05))
		if k < _lag:
			_lag_wait += dt
			if _lag_wait > 0.3:
				_lag = lerpf(_lag, k, 1.0 - exp(-dt / 0.27))
		else:
			_lag = k
			_lag_wait = 0.0
		_boss_lag.size.x = 820.0 * _lag
	if _hint_t > 0:
		_hint_t -= 1
		_hint.modulate.a = move_toward(_hint.modulate.a, 1.0, dt / 0.4)
	else:
		_hint.modulate.a = move_toward(_hint.modulate.a, 0.0, dt / 0.4)

## Score and judgement popups, recycled (no node creation during a fight).
func update_popups(view: View, dt: float) -> void:
	for q in view.popups:
		if _pops.size() >= 14:
			var old: Dictionary = _pops.pop_front()
			(old.l as Label).visible = false
			_pool.append(old.l)
		var l: Label = _pool.pop_back() if _pool.size() > 0 else null
		if l == null:
			l = Kit.label(self, "", 13)
		var st: Array = POP_STYLE.get(q.cls, POP_STYLE.score)
		var ls := LabelSettings.new()
		ls.font = Kit.font(st[2], st[3], st[4], st[1])
		ls.font_size = st[1]
		ls.font_color = st[0]
		ls.shadow_color = Color(st[0].r, st[0].g, st[0].b, 0.6)
		ls.shadow_size = 12
		ls.shadow_offset = Vector2.ZERO
		l.label_settings = ls
		l.text = q.text
		l.visible = true
		l.reset_size()
		_pops.append({"l": l, "t": 0.0, "x": q.x, "y": q.y, "z": q.z})
	view.popups.clear()
	var i := _pops.size() - 1
	while i >= 0:
		var p: Dictionary = _pops[i]
		p.t += dt
		var l: Label = p.l
		if p.t > 0.8:
			l.visible = false
			_pool.append(l)
			_pops.remove_at(i)
			i -= 1
			continue
		var ui := view.project(p.x, p.y + p.t * 1.6, p.z)
		var k: float = (1.4 - p.t * 5.0) if p.t < 0.08 else 1.0
		l.pivot_offset = l.size / 2
		l.scale = Vector2(k, k)
		l.position = ui - l.size / 2
		l.modulate.a = (1.0 - (p.t - 0.55) / 0.25) if p.t > 0.55 else 1.0
		i -= 1

## One point of Resonance: a diamond of light.
class Diamond extends Control:
	var lit := true
	func _init(on: bool) -> void:
		lit = on
		custom_minimum_size = Vector2(16, 26)
		mouse_filter = Control.MOUSE_FILTER_IGNORE
	func _draw() -> void:
		var s := 1.0 if lit else 0.8
		var c := Vector2(8, 13)
		var pts := PackedVector2Array([c + Vector2(0, -13) * s, c + Vector2(8, 0) * s, c + Vector2(0, 13) * s, c + Vector2(-8, 0) * s])
		if lit:
			for k in range(3, 0, -1):
				var g := PackedVector2Array()
				for p in pts:
					g.append(c + (p - c) * (1.0 + k * 0.25))
				draw_colored_polygon(g, Color(0.18, 0.95, 1.0, 0.08))
			draw_polygon(pts, PackedColorArray([Color.WHITE, Color("#97f8ff"), Kit.CYAN, Color("#97f8ff")]))
		else:
			draw_colored_polygon(pts, Color(1, 1, 1, 0.1))
