extends SceneTree
## Visual check of the models: -- <out.png> <what> [frames]
var n := 0
var frames := 40
var out := "/tmp/look.png"
var ama: Colossus
var foe: Colossus
var cam: Camera3D
var what := "ama"

func _init() -> void:
	var a := OS.get_cmdline_user_args()
	out = a[0] if a.size() > 0 else out
	what = a[1] if a.size() > 1 else what
	frames = int(a[2]) if a.size() > 2 else frames
	var root := Node3D.new()
	get_root().add_child(root)
	var we := WorldEnvironment.new()
	var env := Environment.new()
	var sky := Sky.new()
	var sm := ShaderMaterial.new()
	sm.shader = load("res://src/render/shaders/sky.gdshader")
	sky.sky_material = sm
	env.background_mode = Environment.BG_SKY
	env.sky = sky
	env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
	env.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.fog_enabled = true
	env.fog_light_color = Color(0.72, 0.62, 0.52)
	env.fog_density = 0.0009
	env.ambient_light_energy = 0.3
	var sun := DirectionalLight3D.new()
	we.environment = env
	root.add_child(we)
	sun.light_color = Color(1.0, 0.9, 0.78)
	sun.rotation = Vector3(deg_to_rad(-28), deg_to_rad(-130), 0)
	sun.shadow_enabled = true
	sun.light_energy = 2.0
	root.add_child(sun)
	var g := MeshInstance3D.new()
	var pm := PlaneMesh.new()
	pm.size = Vector2(3000, 3000)
	pm.subdivide_width = 120
	pm.subdivide_depth = 120
	g.mesh = pm
	var gm := ShaderMaterial.new()
	gm.shader = load("res://src/render/shaders/ash.gdshader")
	g.material_override = gm
	root.add_child(g)
	ama = Colossus.new(false)
	root.add_child(ama)
	var deck := Deck.new()
	ama.body.add_child(deck)
	for i in 4:
		var pp: Vector2i = March.POSTS[[0, 1, 3, 4][i]]
		deck.crew[i].position = Deck.at(pp.x, pp.y)
	sun.directional_shadow_max_distance = 400.0
	if what.begins_with("fight"):
		foe = Colossus.new(true)
		foe.position = Vector3(0, 0, -90)
		foe.rotation.y = PI
		root.add_child(foe)
	cam = Camera3D.new()
	cam.fov = 55
	cam.far = 4000
	root.add_child(cam)
	match what:
		"ama": cam.look_at_from_position(Vector3(75, 45, -75), Vector3(0, 20, -5))
		"side": cam.look_at_from_position(Vector3(110, 25, 0), Vector3(0, 22, 0))
		"back": cam.look_at_from_position(Vector3(14, 52, 42), Vector3(0, 30, -6))
		_: cam.look_at_from_position(Vector3(150, 45, -45), Vector3(0, 18, -45))

func _process(dt: float) -> bool:
	n += 1
	var st := 0
	if what == "fight_strike":
		st = March.T.WINDUP
	ama.animate(1.0 / 30.0, 0.0 if what.begins_with("fight") else 10.0)
	if foe:
		ama.set_fight(st, 12)
		foe.set_fight(March.T.GRAB_WINDUP if what == "fight_strike" else 0, 10)
		foe.animate(1.0 / 30.0, 0)
	if n == frames:
		await RenderingServer.frame_post_draw
		get_root().get_texture().get_image().save_png(out)
		quit()
	return false
