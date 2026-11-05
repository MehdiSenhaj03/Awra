class_name Timeline
extends Control
## The loop as a strip: one row per echo (its colour, where it acted), the present, the alarm.
## Hovering previews any instant (the simulation is deterministic); clicking rewinds there.

signal hover(frame: int)
signal pick(frame: int)

var run: Run
var now := 0
var hover_frame := -1
var marks: Array = [] # [{slot, frame, kind}]
var live: Array = []
var alarm_frame := -1

func _init() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	custom_minimum_size = Vector2(1200, 0)

func _frame_at(x: float) -> int:
	if run == null:
		return 0
	return clampi(int((x - 40.0) / (size.x - 60.0) * run.loop_frames), 0, run.loop_frames)

func _gui_input(e: InputEvent) -> void:
	if e is InputEventMouseMotion:
		hover_frame = _frame_at(e.position.x)
		hover.emit(hover_frame)
		queue_redraw()
	elif e is InputEventMouseButton and e.pressed and e.button_index == MOUSE_BUTTON_LEFT:
		pick.emit(_frame_at(e.position.x))
		accept_event()

func _notification(what: int) -> void:
	if what == NOTIFICATION_MOUSE_EXIT:
		hover_frame = -1
		hover.emit(-1)
		queue_redraw()

func _draw() -> void:
	if run == null:
		return
	var w := size.x - 60.0
	var x0 := 40.0
	var n := run.rec.size()
	var rh := 14.0
	var top := 18.0
	draw_rect(Rect2(0, 0, size.x, size.y), Color(0.02, 0.025, 0.05, 0.82))
	draw_rect(Rect2(0, 0, size.x, size.y), Color(1, 1, 1, 0.08), false, 1.0)
	# Seconds.
	var secs := run.loop_frames / 60
	for s in range(0, secs + 1, 5):
		var x := x0 + w * s * 60.0 / run.loop_frames
		draw_line(Vector2(x, 10), Vector2(x, size.y - 6), Color(1, 1, 1, 0.07), 1.0)
		draw_string(Kit.font("mono", 400, 0.0, 11), Vector2(x + 3, 12), "%d" % s, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color(1, 1, 1, 0.35))
	for i in n:
		var c: Color = Mats.srgb(Props.ECHO_COLORS[i % 6])
		var y := top + i * (rh + 6)
		var is_live := live.has(i)
		var upto := now if is_live else run.loop_frames
		draw_rect(Rect2(x0, y, w, rh), Color(c.r, c.g, c.b, 0.08))
		draw_rect(Rect2(x0, y, w * upto / run.loop_frames, rh), Color(c.r, c.g, c.b, 0.55 if is_live else 0.3))
		draw_circle(Vector2(x0 - 16, y + rh / 2), 6, c)
		if is_live:
			draw_arc(Vector2(x0 - 16, y + rh / 2), 9, 0, TAU, 20, Color(1, 1, 1, 0.8), 1.5)
	for m: Dictionary in marks:
		var y := top + int(m.slot) * (rh + 6)
		var x := x0 + w * float(m.frame) / run.loop_frames
		draw_rect(Rect2(x - 2, y - 2, 4, rh + 4), Color(1, 0.85, 0.4))
	if alarm_frame >= 0:
		var x := x0 + w * float(alarm_frame) / run.loop_frames
		draw_line(Vector2(x, 4), Vector2(x, size.y - 4), Color(1, 0.2, 0.25), 3.0)
	var px := x0 + w * float(now) / run.loop_frames
	draw_line(Vector2(px, 2), Vector2(px, size.y - 2), Color(1, 1, 1, 0.95), 2.0)
	if hover_frame >= 0:
		var hx := x0 + w * float(hover_frame) / run.loop_frames
		draw_line(Vector2(hx, 2), Vector2(hx, size.y - 2), Color(0.18, 0.95, 1.0, 0.7), 1.0)
		draw_string(Kit.font("mono", 600, 0.0, 12), Vector2(hx + 4, size.y - 6), "%.1f s" % (hover_frame / 60.0), HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Kit.CYAN)
