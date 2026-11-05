extends Node
## Renders a vault with planned echoes and saves screenshots. Args: level path, seconds, out prefix, plans json.
var sim: Sim
var view: View
var planners: Array = []
var prefix := "/tmp/shot"
var total := 6.0
var t := 0.0
var shots: Array = []
var fast := 2

func _ready() -> void:
	var a := OS.get_cmdline_user_args()
	var lv := Level.new(JSON.parse_string(FileAccess.get_file_as_string(a[0])))
	total = float(a[1])
	prefix = a[2]
	var plans: Array = JSON.parse_string(a[3]) if a.size() > 3 else []
	sim = Sim.new(lv)
	sim.reset_echoes(maxi(1, plans.size()))
	for i in plans.size():
		planners.append(Planner.new(plans[i], i))
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
	view.post = pm
	view.build(lv, sim)
	view.set_quality(2)
	for i in 4:
		shots.append(total * (i + 1) / 4.0)

func _process(_dt: float) -> void:
	var dt := fast / 60.0
	t += dt
	for k in fast:
		view.capture(sim)
		var inp := PackedByteArray()
		inp.resize(sim.echoes.size())
		for i in planners.size():
			inp[i] = planners[i].next(sim)
		sim.step(inp)
		view.handle(sim.events)
	view.render(sim, null, 1.0, dt, [planners.size() - 1])
	if shots.size() > 0 and t >= shots[0]:
		shots.pop_front()
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png("%s_%d.png" % [prefix, 4 - shots.size()])
	if t > total + 0.1:
		print("over=", sim.over, " frame=", sim.frame)
		get_tree().quit()
