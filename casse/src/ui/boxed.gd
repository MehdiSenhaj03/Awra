class_name Boxed
extends Control
## A clickable box with a background and a 1 px border whose colours follow its state (cards, stage buttons).

signal clicked
signal hovered
signal right_clicked

var bg := Color(1, 1, 1, 0.04)
var border := Color(1, 1, 1, 0.09)
var bg_on := Color(0.18, 0.95, 1.0, 0.1)
var border_on := Kit.CYAN
var inset := Color(0, 0, 0, 0)
var lift := 2.0
var on := false
var _k := 0.0

func _init() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	mouse_entered.connect(func() -> void: hovered.emit())

func set_on(v: bool) -> void:
	on = v
	queue_redraw()

func _gui_input(e: InputEvent) -> void:
	if e is InputEventMouseButton and e.pressed:
		accept_event()
		if e.button_index == MOUSE_BUTTON_LEFT:
			clicked.emit()
		elif e.button_index == MOUSE_BUTTON_RIGHT:
			right_clicked.emit()

func _process(dt: float) -> void:
	var t := 1.0 if on else 0.0
	if absf(_k - t) > 0.002:
		_k += (t - _k) * (1.0 - exp(-dt * 16.0))
		for c in get_children():
			if c is Control:
				(c as Control).position.y = (c as Control).get_meta("y0", (c as Control).position.y) - lift * _k
		queue_redraw()

func _ready() -> void:
	for c in get_children():
		if c is Control:
			c.set_meta("y0", (c as Control).position.y)

func _draw() -> void:
	var r := Rect2(Vector2(0, -lift * _k), size)
	draw_rect(r, bg.lerp(bg_on, _k))
	draw_rect(r, border.lerp(border_on, _k), false, 1.0)
	if inset.a > 0.0:
		draw_rect(r.grow(-1.5), inset, false, 1.0)
