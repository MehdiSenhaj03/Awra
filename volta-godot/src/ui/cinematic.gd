class_name CinematicScreen
extends Screen
## Full-screen illustrations with captions; each shot lasts a few seconds, any key advances, a long press skips.

var _shots: Array
var _on_done: Callable
var _i := -1
var _t := 0.0
var _imgs: Array[TextureRect] = []
var _txt: Label
var _held := 0.0

func _init(shots: Array, on_done: Callable) -> void:
	super()
	_shots = shots
	_on_done = on_done
	Kit.full(Kit.rect(self, Color.BLACK))
	for s: Dictionary in shots:
		var r := Kit.image(self, s.image)
		Kit.full(r)
		r.offset_left = -Kit.W * 0.05
		r.offset_right = Kit.W * 0.05
		r.offset_top = -Kit.H * 0.05
		r.offset_bottom = Kit.H * 0.05
		r.pivot_offset = Vector2(Kit.W * 0.55, Kit.H * 0.55)
		r.modulate.a = 0.0
		_imgs.append(r)
	var fade := TextureRect.new()
	var g := GradientTexture2D.new()
	var gr := Gradient.new()
	gr.offsets = PackedFloat32Array([0.0, 0.5, 0.85, 1.0])
	gr.colors = PackedColorArray([Color(0, 0, 0, 0), Color(0, 0, 0, 0), Color(0, 0, 0, 0.8), Color(0, 0, 0, 1)])
	g.gradient = gr
	g.fill_to = Vector2(0, 1)
	g.width = 4
	g.height = 256
	fade.texture = g
	fade.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	fade.stretch_mode = TextureRect.STRETCH_SCALE
	fade.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(fade)
	Kit.full(fade)
	Kit.rect(self, Color.BLACK, Vector2.ZERO, Vector2(Kit.W, Kit.H * 0.11))
	Kit.rect(self, Color.BLACK, Vector2(0, Kit.H * 0.89), Vector2(Kit.W, Kit.H * 0.11 + 2))
	_txt = Kit.label(self, "", 24, Color("#f1f5ff"), "body", 600, 0.0, Color(0, 0, 0, 0.9), 16)
	_txt.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_txt.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_txt.size = Vector2(980, 160)
	_txt.position = Vector2((Kit.W - 980) / 2, Kit.H * 0.86 - 160)
	_txt.vertical_alignment = VERTICAL_ALIGNMENT_BOTTOM
	var sk := Kit.label(self, "%s ▶▶" % I18n.t("common.skip"), 11, Color(1, 1, 1, 0.38), "display", 500, 0.3)
	sk.position = Vector2(Kit.W - 200, Kit.H * 0.13)
	_next.call_deferred()

func _gui_input(e: InputEvent) -> void:
	if e is InputEventMouseButton and e.pressed:
		_next()

func _next() -> void:
	_i += 1
	if _i >= _shots.size():
		_i = 999
		_on_done.call()
		return
	for k in _imgs.size():
		var r := _imgs[k]
		var tw := r.create_tween().set_parallel()
		tw.tween_property(r, "modulate:a", 1.0 if k == _i else 0.0, 1.4)
		if k == _i:
			r.scale = Vector2(1.12, 1.12)
			tw.tween_property(r, "scale", Vector2.ONE, 14.0).set_ease(Tween.EASE_OUT)
	_txt.modulate.a = 0.0
	_txt.text = _shots[_i].text
	var tt := _txt.create_tween()
	tt.tween_interval(0.6)
	tt.tween_property(_txt, "modulate:a", 1.0, 0.8)
	_t = 0.0

func update(dt: float) -> void:
	if _i >= _shots.size() or _i < 0:
		return
	_t += dt
	var ln := 3.0 + String(_shots[_i].text).length() / 22.0
	if _t > ln:
		_next()
		return
	if Keys.pressed("confirm") or Keys.pressed("fire") or Keys.pressed("dash"):
		_next()
		return
	_held = _held + dt if (Keys.down("back") or Keys.down("pause")) else 0.0
	if _held > 0.6:
		_i = 999
		_on_done.call()
