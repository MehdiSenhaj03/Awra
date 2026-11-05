class_name MenuItem
extends Control
## One menu entry: display font, a cyan bar and a glow when focused, slides right; optional subtitle and value.

signal clicked

var action: Callable
var adjust: Callable
var disabled := false
var on := false
var _label: Label
var _sub: Label
var _value: Label
var _x := 0.0
var _k := 0.0

func _init(text: String, sub: String = "", value: String = "", size: int = 18, width: int = 0) -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	_label = Kit.label(self, text, size, Kit.SOFT, "display", 500, 0.22)
	_label.position = Vector2(18, 10)
	var h := size * 1.5 + 20
	if sub != "":
		_sub = Kit.label(self, sub, 12, Kit.MUTED, "body", 400, 0.04)
		_sub.position = Vector2(18, 12 + size * 1.4)
		h += 18
	if value != "" or width > 0:
		_value = Kit.label(self, value, int(size * 0.95), Kit.CYAN, "mono", 400)
		_value.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	var w := float(width) if width > 0 else maxf(_label.get_minimum_size().x + 60, _sub.get_minimum_size().x + 60 if _sub else 0.0)
	custom_minimum_size = Vector2(w, h)

func _ready() -> void:
	if _value:
		_value.position = Vector2(size.x - 260, 10)
		_value.size = Vector2(230, _label.size.y)
	_refresh()

func set_text(s: String) -> void:
	_label.text = s

func set_value(s: String) -> void:
	if _value:
		_value.text = s

func set_on(v: bool) -> void:
	on = v
	_refresh()

func _refresh() -> void:
	var ls := _label.label_settings
	if disabled:
		ls.font_color = Color(1, 1, 1, 0.22)
		ls.shadow_size = 0
	elif on:
		ls.font_color = Color.WHITE
		ls.shadow_color = Color(0.18, 0.95, 1.0, 0.6)
		ls.shadow_size = 10
		ls.shadow_offset = Vector2.ZERO
	else:
		ls.font_color = Kit.SOFT
		ls.shadow_size = 0
	queue_redraw()

func _gui_input(e: InputEvent) -> void:
	if e is InputEventMouseButton and e.pressed and e.button_index == MOUSE_BUTTON_LEFT:
		accept_event()
		clicked.emit()

func _process(dt: float) -> void:
	var target := 1.0 if on else 0.0
	if absf(_k - target) > 0.001:
		_k += (target - _k) * (1.0 - exp(-dt * 18.0))
		_x = 8.0 * _k
		_label.position.x = 18 + _x
		if _sub:
			_sub.position.x = 18 + _x
		queue_redraw()

func _draw() -> void:
	if _k < 0.01:
		return
	var w := size.x
	var h := size.y
	# Slanted gradient strip and the bar.
	var pts := PackedVector2Array([Vector2(_x, 0), Vector2(_x + w, 0), Vector2(_x + w - 14, h), Vector2(_x, h)])
	var cols := PackedColorArray([Color(0.18, 0.95, 1.0, 0.15 * _k), Color(0.18, 0.95, 1.0, 0.0), Color(0.18, 0.95, 1.0, 0.0), Color(0.18, 0.95, 1.0, 0.15 * _k)])
	draw_polygon(pts, cols)
	var bh := h * 0.7 * _k
	draw_rect(Rect2(_x, (h - bh) / 2, 3, bh), Kit.CYAN * Color(1.5, 1.5, 1.5, _k))
