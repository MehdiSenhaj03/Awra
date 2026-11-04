class_name TitleScreen
extends Screen
## Title screen: the painted key art, the animated logo, then the menu.

var _items: Array
var _on_wake: Callable
var _menu: UIMenu
var _press: Label
var _wrap: Control

func _init(items: Array, on_wake: Callable, awake: bool) -> void:
	super()
	_items = items
	_on_wake = on_wake
	var bg := Kit.background(self, "bg_title")
	if bg.texture == null:
		bg.texture = Kit.tex("key_art")
	# Shade: dark on the left (behind the menu), vignette.
	var shade := TextureRect.new()
	var g := GradientTexture2D.new()
	var gr := Gradient.new()
	gr.offsets = PackedFloat32Array([0.0, 0.35, 0.6, 1.0])
	gr.colors = PackedColorArray([Color(0, 0, 0, 0.88), Color(0, 0, 0, 0.5), Color(0, 0, 0, 0.0), Color(0, 0, 0, 0.25)])
	g.gradient = gr
	g.width = 256
	g.height = 4
	shade.texture = g
	shade.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	shade.stretch_mode = TextureRect.STRETCH_SCALE
	shade.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(shade)
	Kit.full(shade)
	var scan := ColorRect.new()
	var sm := ShaderMaterial.new()
	sm.shader = preload("res://src/ui/shaders/scan.gdshader")
	scan.material = sm
	scan.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(scan)
	Kit.full(scan)
	# Logo.
	var logo := Control.new()
	logo.position = Vector2(Kit.W * 0.07, Kit.H * 0.2)
	logo.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(logo)
	var h1 := Kit.label(logo, "VOLTA", 180, Color.WHITE, "display", 800, 0.08)
	var lm := ShaderMaterial.new()
	lm.shader = preload("res://src/ui/shaders/gradient_text.gdshader")
	lm.set_shader_parameter("speed", -0.125)
	lm.set_shader_parameter("size", Vector2(820, 200))
	h1.material = lm
	var tag := Kit.label(logo, I18n.t("title.tagline").to_upper(), 18, Color("#cfe9ff"), "display", 500, 0.7, Color(0.18, 0.95, 1.0, 0.7), 12)
	tag.position = Vector2(10, 228)
	var line := ColorRect.new()
	line.position = Vector2(0, 280)
	line.size = Vector2(0, 1)
	line.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var lmat := ShaderMaterial.new()
	lmat.shader = preload("res://src/ui/shaders/line.gdshader")
	line.material = lmat
	logo.add_child(line)
	var tw := line.create_tween()
	tw.tween_interval(0.4)
	tw.tween_property(line, "size:x", 560.0, 1.2).set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
	_press = Kit.label(self, I18n.t("title.press").to_upper(), 13, Color(1, 1, 1, 0.7), "display", 500, 0.5)
	_press.position = Vector2(Kit.W * 0.07, Kit.H * 0.84 - 13)
	var bl := _press.create_tween().set_loops()
	bl.tween_property(_press, "modulate:a", 0.25, 0.8).set_trans(Tween.TRANS_SINE)
	bl.tween_property(_press, "modulate:a", 1.0, 0.8).set_trans(Tween.TRANS_SINE)
	_wrap = Control.new()
	_wrap.position = Vector2(Kit.W * 0.07, Kit.H * 0.5)
	_wrap.size = Vector2(800, Kit.H * 0.38)
	_wrap.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_wrap)
	var corner := Kit.label(self, "v1.0 · 2026", 11, Color(1, 1, 1, 0.32), "mono", 400, 0.1)
	corner.position = Vector2(Kit.W - 130, Kit.H - 36)
	if awake:
		_wake.call_deferred()

func _gui_input(e: InputEvent) -> void:
	if e is InputEventMouseButton and e.pressed:
		_wake()

func warn(text: String) -> void:
	var p := Kit.panel(self)
	p.position = Vector2(Kit.W * 0.07, Kit.H * 0.1)
	var sb := (p.get_theme_stylebox("panel") as StyleBoxFlat).duplicate() as StyleBoxFlat
	sb.bg_color = Color("#2a0f05cc")
	sb.border_color = Color("#ff7a2f88")
	p.add_theme_stylebox_override("panel", sb)
	var m := Kit.margin(p, 14, 10, 14, 10)
	var l := Kit.label(m, text, 13, Color("#ffd2b0"))
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	l.custom_minimum_size.x = 540

func _wake() -> void:
	if _menu:
		return
	_on_wake.call()
	_press.queue_free()
	_menu = UIMenu.new(_wrap)
	_menu.root.position = Vector2.ZERO
	for it: Dictionary in _items:
		_menu.add(it.label, it.action, {"disabled": it.get("disabled", false), "sub": it.get("sub", "")})
	# Anchor the menu to the bottom of its area.
	_menu.root.position.y = _wrap.size.y - _menu.root.get_combined_minimum_size().y

func update(_dt: float) -> void:
	if _menu == null:
		if Keys.pressed("confirm") or Keys.pressed("fire") or Keys.pressed("dash") or Keys.pressed("pause"):
			_wake()
		return
	_menu.handle()
