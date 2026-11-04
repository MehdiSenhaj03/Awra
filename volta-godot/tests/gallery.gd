extends Node
## All models in a grid under the game camera angle, for visual checks.

var view: View
var frames := 0

func _ready() -> void:
	view = View.new()
	add_child(view)
	var w := World.new({"shape": "rect", "w": 30, "h": 20, "pillars": []}, 128.0, 1)
	view.set_stage(w, 0x2ef2ff, 0xff2fb9, 0x02040c, null)
	view.set_quality(2)
	var looks: Array = Models.LOOKS
	var i := 0
	for look: String in looks:
		var boss := ["ostinato", "crescendo", "fugue", "rubato", "staccato", "fermata"].has(look)
		var r := 0.6 * 1.35
		if boss:
			r = 1.2
		var m := Models.enemy(look, r, 0xff2fb9 if i % 2 == 0 else 0x2ef2ff, look == "mote")
		m.root.position = Vector3((i % 7) * 5.0 - 15.0, 0, int(i / 7) * 6.0 - 6.0)
		add_child(m.root)
		i += 1
	var pm := Models.player()
	pm.root.position = Vector3(-15, 0, 12)
	pm.root.scale = Vector3.ONE * 1.6
	add_child(pm.root)

func _process(_dt: float) -> void:
	frames += 1
	view.camera.look_at_from_position(Vector3(0, 26, 22), Vector3(0, 0, 2))
	view.camera.fov = 50
	view.update_mirror()
	if frames == 8:
		await RenderingServer.frame_post_draw
		get_viewport().get_texture().get_image().save_png("/tmp/shots/gallery.png")
		get_tree().quit()
