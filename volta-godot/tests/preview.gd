extends Node
## Renders a stage driven by the bot and saves screenshots (visual checks under xvfb).
## godot --path . --resolution 960x540 res://tests/preview.tscn -- <stage> <seconds> <out_prefix>

var w: World
var run: StageRunner
var view: View
var acc := 0.0
var frames := 0
var shots: Array = []
var prefix := "/tmp/shot"
var total := 8.0
var t := 0.0

func _ready() -> void:
	var args := OS.get_cmdline_user_args()
	var id: String = args[0] if args.size() > 0 else "s1_1"
	total = float(args[1]) if args.size() > 1 else 8.0
	prefix = args[2] if args.size() > 2 else "/tmp/shot"
	var d := Stages.get_def(id)
	w = World.new(d.arena, d.bpm, 3)
	w.god = true
	w.player.y = 4.0
	run = StageRunner.new(w, d)
	view = View.new()
	add_child(view)
	var layer := CanvasLayer.new()
	var rect := ColorRect.new()
	rect.set_anchors_preset(Control.PRESET_FULL_RECT)
	rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var pm := ShaderMaterial.new()
	pm.shader = preload("res://src/render/shaders/post.gdshader")
	rect.material = pm
	layer.add_child(rect)
	add_child(layer)
	view.set_post(pm)
	var sector: int = d.get("sector", 1)
	var looks := {1: [0x2ef2ff, 0xff2fb9, 0x02040c, "bg_seuil"], 2: [0x40ffb0, 0x2ef2ff, 0x020a08, "bg_jardin"], 3: [0xffa040, 0xff2fb9, 0x0a0402, "bg_fonderie"], 4: [0xc8d4ff, 0x9b5cff, 0x050510, "bg_ocean"], 5: [0xff2fb9, 0x9b5cff, 0x0a0210, "bg_ville"], 6: [0xffffff, 0xffc34a, 0x050505, "bg_coeur"]}
	var lk: Array = looks.get(sector, looks[1])
	view.set_stage(w, lk[0], lk[1], lk[2], load("res://assets/img/%s.webp" % lk[3]))
	view.set_quality(2)
	for i in 4:
		shots.append(total * (i + 1) / 4.0)

func _process(dt: float) -> void:
	dt = 1.0 / 60.0
	t += dt
	var t0 := Time.get_ticks_usec()
	if w.over == "":
		w.update(Bot.controls(w))
		run.update()
	view.handle(w.events, w)
	w.events.clear()
	var t1 := Time.get_ticks_usec()
	view.render(w, 1.0, dt)
	var t2 := Time.get_ticks_usec()
	frames += 1
	if frames % 60 == 0:
		print("t=%.1f sim=%.2fms view=%.2fms enemies=%d bullets=%d fps=%d" % [t, (t1 - t0) / 1000.0, (t2 - t1) / 1000.0, w.enemies.size(), w.bullets.n, Engine.get_frames_per_second()])
	if shots.size() > 0 and t >= shots[0]:
		shots.pop_front()
		await RenderingServer.frame_post_draw
		var img := get_viewport().get_texture().get_image()
		img.save_png("%s_%d.png" % [prefix, 4 - shots.size()])
	if t > total + 0.2:
		get_tree().quit()
