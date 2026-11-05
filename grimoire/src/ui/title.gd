class_name TitleScreen
extends Screen
## Title: the scriptorium tower at dusk, the logo, then the menu.

var _items: Array
var _on_wake: Callable
var _menu: UIMenu
var _press: Label
var _wrap: Control

func _init(items: Array, on_wake: Callable, awake: bool) -> void:
	super()
	_items = items
	_on_wake = on_wake
	Kit.background(self, "bg_title")
	var shade := TextureRect.new()
	var g := GradientTexture2D.new()
	var gr := Gradient.new()
	gr.offsets = PackedFloat32Array([0.0, 0.38, 0.62, 1.0])
	gr.colors = PackedColorArray([Color(0, 0, 0, 0.9), Color(0, 0, 0, 0.55), Color(0, 0, 0, 0.0), Color(0, 0, 0, 0.2)])
	g.gradient = gr
	g.width = 256
	g.height = 4
	shade.texture = g
	shade.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	shade.stretch_mode = TextureRect.STRETCH_SCALE
	shade.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(shade)
	Kit.full(shade)
	var logo := Control.new()
	logo.position = Vector2(Kit.W * 0.07, Kit.H * 0.14)
	logo.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(logo)
	var h1 := Kit.label(logo, "Grimoire", 170, Color.WHITE, "deco", 400, 0.02, Color(1, 0.6, 0.2, 0.5), 28)
	var lm := ShaderMaterial.new()
	lm.shader = preload("res://src/ui/shaders/gradient_text.gdshader")
	lm.set_shader_parameter("vertical", 1.0)
	lm.set_shader_parameter("c0", Color("#fff3d8"))
	lm.set_shader_parameter("c1", Color("#c8892e"))
	lm.set_shader_parameter("size", Vector2(1100, 200))
	h1.material = lm
	var line := Kit.rect(logo, Kit.BRASS, Vector2(6, 206), Vector2(0, 2))
	var tw := line.create_tween()
	tw.tween_interval(0.3)
	tw.tween_property(line, "size:x", 620.0, 1.2).set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_CUBIC)
	var tag := Kit.label(logo, I18n.t("title.tagline"), 22, Kit.CREAM, "deco", 400, 0.04, Color(0, 0, 0, 0.8), 10)
	tag.position = Vector2(8, 226)
	tag.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	tag.size = Vector2(720, 70)
	_press = Kit.label(self, I18n.t("title.press").to_upper(), 14, Color(1, 1, 1, 0.75), "body", 600, 0.4)
	_press.position = Vector2(Kit.W * 0.07, Kit.H * 0.84)
	var bl := _press.create_tween().set_loops()
	bl.tween_property(_press, "modulate:a", 0.25, 0.8).set_trans(Tween.TRANS_SINE)
	bl.tween_property(_press, "modulate:a", 1.0, 0.8).set_trans(Tween.TRANS_SINE)
	_wrap = Control.new()
	_wrap.position = Vector2(Kit.W * 0.07, Kit.H * 0.46)
	_wrap.size = Vector2(800, Kit.H * 0.46)
	_wrap.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_wrap)
	var corner := Kit.label(self, "v1.0", 12, Color(1, 1, 1, 0.35), "mono", 400, 0.1)
	corner.position = Vector2(Kit.W - 80, Kit.H - 36)
	if awake:
		_wake.call_deferred()

func _gui_input(e: InputEvent) -> void:
	if e is InputEventMouseButton and e.pressed:
		_wake()

func warn(text: String) -> void:
	var l := Kit.label(self, text, 14, Color("#ffd2b0"), "body", 400, 0.0, Color(0, 0, 0, 0.9), 8)
	l.position = Vector2(Kit.W * 0.07, 30)
	l.size = Vector2(900, 40)
	l.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART

func _wake() -> void:
	if _menu:
		return
	_on_wake.call()
	_press.queue_free()
	_menu = UIMenu.new(_wrap)
	for it: Dictionary in _items:
		_menu.add(it.label, it.action, {"disabled": it.get("disabled", false), "sub": it.get("sub", ""), "size": 22})

func update(_dt: float) -> void:
	if _menu == null:
		if Keys.pressed("confirm") or Keys.pressed("act") or Keys.pressed("pause"):
			_wake()
		return
	_menu.handle()
