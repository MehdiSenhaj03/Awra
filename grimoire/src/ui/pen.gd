class_name Pen
extends RefCounted
## One player's quill: the mouse (press and drag), or a gamepad (the left stick moves the nib, the right trigger
## or A draws). Collects strokes; when the hand rests a moment, the glyph is complete.

signal glyph(strokes: Array, center: Vector2, size: float)
signal step(dir: int)

const IDLE := 0.33          # seconds without drawing that close a glyph
const MIN_SIZE := 26.0

var source := "mouse"       # "mouse" or "pad<N>"
var strokes: Array = []     # Array of PackedVector2Array
var cursor := Vector2(960, 640)
var drawing := false
var enabled := true
var area := Rect2(0, 0, 1920, 1080)
var _idle := 0.0
var _was_down := false
var _step_prev := 0

func _init(src: String) -> void:
	source = src

func cancel() -> void:
	strokes.clear()
	drawing = false
	_idle = 0.0

## Called every frame with the mouse position in canvas coordinates.
func update(dt: float, mouse: Vector2) -> void:
	var down := false
	if source == "mouse":
		cursor = mouse
		down = Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)
		if Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT) and not strokes.is_empty():
			cancel()
			return
		var sd := 0
		if Input.is_action_just_pressed("step_up"):
			sd = -1
		elif Input.is_action_just_pressed("step_down"):
			sd = 1
		if sd != 0:
			step.emit(sd)
	else:
		var dv := int(source.substr(3))
		var st := Vector2(Input.get_joy_axis(dv, JOY_AXIS_LEFT_X), Input.get_joy_axis(dv, JOY_AXIS_LEFT_Y))
		if st.length() > 0.12:
			cursor += st * st.length() * dt * 1500.0
		cursor = cursor.clamp(area.position, area.end)
		down = Input.get_joy_axis(dv, JOY_AXIS_TRIGGER_RIGHT) > 0.35 or Input.is_joy_button_pressed(dv, JOY_BUTTON_A)
		var sd := 0
		if Input.is_joy_button_pressed(dv, JOY_BUTTON_DPAD_UP) or Input.is_joy_button_pressed(dv, JOY_BUTTON_LEFT_SHOULDER):
			sd = -1
		elif Input.is_joy_button_pressed(dv, JOY_BUTTON_DPAD_DOWN) or Input.is_joy_button_pressed(dv, JOY_BUTTON_RIGHT_SHOULDER):
			sd = 1
		if sd != 0 and _step_prev == 0:
			step.emit(sd)
		_step_prev = sd
		if Input.is_joy_button_pressed(dv, JOY_BUTTON_B) and not strokes.is_empty():
			cancel()
			return
	if not enabled:
		_was_down = down
		return
	if down and not _was_down and area.has_point(cursor):
		strokes.append(PackedVector2Array([cursor]))
		drawing = true
	elif down and drawing:
		var s: PackedVector2Array = strokes[strokes.size() - 1]
		if s[s.size() - 1].distance_to(cursor) > 2.5:
			s.append(cursor)
			strokes[strokes.size() - 1] = s
	elif not down and drawing:
		drawing = false
		_idle = 0.0
	_was_down = down
	if not drawing and not strokes.is_empty():
		_idle += dt
		if _idle >= IDLE:
			_finish()

func _finish() -> void:
	var mn := Vector2(INF, INF)
	var mx := Vector2(-INF, -INF)
	var arr: Array = []
	for s: PackedVector2Array in strokes:
		var a: Array = []
		for p in s:
			mn = mn.min(p)
			mx = mx.max(p)
			a.append(p)
		arr.append(a)
	strokes.clear()
	var size := (mx - mn).length()
	if size < MIN_SIZE:
		return
	# A lone straight vertical stroke is a step.
	var sd := Glyphs.step_of(arr, 90.0)
	if sd != 0:
		step.emit(sd)
		return
	glyph.emit(arr, (mn + mx) * 0.5, size)
