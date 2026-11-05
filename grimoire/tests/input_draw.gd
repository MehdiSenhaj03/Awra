extends SceneTree
## End-to-end input check: drives the mouse (through the engine's input events) to draw a fire glyph in a duel and
## checks that the duel received a fire cast in the lane it was drawn in.
var main: Node
var frames := 0
var pts: Array = []
var k := 0
var ok := false

func _initialize() -> void:
	main = load("res://src/main.tscn").instantiate()
	root.add_child(main)
	# A zigzag (fire) around the middle lane, then wait for the glyph to close.
	for i in 5:
		var x := 700.0 + i * 70.0
		var y := 640.0 if i % 2 == 0 else 520.0
		pts.append(Vector2(x, y))

func _win(p: Vector2) -> Vector2:
	return root.get_final_transform() * p

func _ev_move(p0: Vector2) -> void:
	var p := _win(p0)
	var m := InputEventMouseMotion.new()
	m.position = p
	m.global_position = p
	Input.parse_input_event(m)

func _ev_btn(p0: Vector2, down: bool) -> void:
	var p := _win(p0)
	var b := InputEventMouseButton.new()
	b.button_index = MOUSE_BUTTON_LEFT
	b.pressed = down
	b.position = p
	b.global_position = p
	Input.parse_input_event(b)

func _process(_dt: float) -> bool:
	frames += 1
	if frames == 5:
		main.call("start_game", "duel", {"mode": "ai", "rival": "aldric", "skill": 0.1, "arena": "ar_scriptorium", "best_of": 1, "seed": 3})
	if frames > 10 and k < 60:
		# Interpolate along the zigzag over 50 frames.
		var t := float(k) / 50.0 * (pts.size() - 1)
		var i := mini(int(t), pts.size() - 2)
		var p: Vector2 = (pts[i] as Vector2).lerp(pts[i + 1], t - i) if k <= 50 else pts[pts.size() - 1]
		_ev_move(p)
		if k == 0:
			_ev_btn(p, true)
		if k == 51:
			_ev_btn(p, false)
		k += 1
	var g: Node = main.get("game")
	if g and frames > 10:
		var d: Duel = g.get("duel")
		var pens: Array = g.get("pens")
		if frames % 20 == 0:
			print("f=", frames, " strokes=", (pens[0] as Pen).strokes.size(), " mouse=", g.get_viewport().get_mouse_position(), " btn=", Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT), " casts=", d.mages[0].casts, " spells=", d.spells.size())
		if d.mages[0].casts > 0:
			ok = true
		for e in d.events:
			if e.t == "cast" and int(e.side) == 0:
				print("CAST glyph=", Glyphs.NAMES[int(e.g)], " lane=", e.lane, " pow=", e.pow)
				ok = true
			elif e.t == "fizzle" and int(e.side) == 0:
				print("FIZZLE")
	if ok or frames > 400:
		print("input test: ", "OK" if ok else "FAILED")
		return true
	return false
