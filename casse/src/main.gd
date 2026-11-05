extends Node
## CASSE: boot, screens, the play loop (simulation at a fixed 60 Hz in the physics step).

var view: View
var ui: Control
var heist: Heist = null
var settings: Dictionary = {"music": 0.7, "sfx": 0.8, "quality": 2, "lang": "fr", "shake": 1.0}
var _post: ShaderMaterial
var _shots := ""
var _seconds := 0.0
var _clock := 0.0
var _shot_i := 0

func _ready() -> void:
	Keys.apply(null)
	I18n.set_lang(settings.lang)
	view = View.new()
	add_child(view)
	var post_layer := CanvasLayer.new()
	add_child(post_layer)
	var pr := ColorRect.new()
	_post = ShaderMaterial.new()
	_post.shader = preload("res://src/render/shaders/post.gdshader")
	pr.material = _post
	pr.mouse_filter = Control.MOUSE_FILTER_IGNORE
	post_layer.add_child(pr)
	Kit.full(pr)
	view.post = _post
	var layer := CanvasLayer.new()
	layer.layer = 1
	add_child(layer)
	ui = Control.new()
	ui.mouse_filter = Control.MOUSE_FILTER_IGNORE
	layer.add_child(ui)
	Kit.full(ui)
	view.set_quality(settings.quality)
	var a := OS.get_cmdline_user_args()
	var lvp := "res://levels/demo.json"
	var plans: Array = []
	for i in a.size():
		match a[i]:
			"--level": lvp = a[i + 1]
			"--plans": plans = JSON.parse_string(a[i + 1])
			"--shots": _shots = a[i + 1]
			"--seconds": _seconds = float(a[i + 1])
	var lv := Level.new(JSON.parse_string(FileAccess.get_file_as_string(lvp)))
	heist = Heist.new(lv, view, 1, ["plan"] if plans.size() > 0 else ["solo"])
	heist.plans = plans
	ui.add_child(heist)

func _physics_process(_d: float) -> void:
	if heist:
		heist.step()

func _process(dt: float) -> void:
	_clock += dt
	if heist:
		heist.frame(minf(dt, 0.1))
	if _shots != "" and _seconds > 0.0:
		if _clock >= _seconds * (_shot_i + 1) / 4.0 and _shot_i < 4:
			_shot_i += 1
			_capture("%s_%d.png" % [_shots, _shot_i])
		if _clock > _seconds + 0.3:
			get_tree().quit()

func _capture(path: String) -> void:
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(path)
