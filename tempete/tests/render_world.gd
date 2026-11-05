extends SceneTree
## Simulates a chase for a while with a computer team, then renders it. -- <seconds simulated> <out> [cam: chase|photo|sky]
var chase: Chase
var world: World
var cam: Camera3D
var ai: ChaserAI
var frames := 0
var args: PackedStringArray

func _initialize() -> void:
	args = OS.get_cmdline_user_args()
	var day := {"seed": 4, "minutes": 9, "storms": [
		{"at": 0, "x": 2_500_000, "y": 3_000_000, "sev": 4, "dir": 560, "speed": 14, "tornadoes": 2},
	]}
	chase = Chase.new(day, 1)
	ai = ChaserAI.new(chase, 0, 1.0, 5)
	var secs := int(args[0]) if args.size() > 0 else 60
	for i in secs * 60:
		chase.step([ai.update()])
	world = World.new(chase)
	root.add_child(world)
	world.update_wet()
	cam = Camera3D.new()
	cam.far = 40000.0
	cam.fov = 62.0
	root.add_child(cam)

func _process(dt: float) -> bool:
	frames += 1
	chase.step([ai.update()])
	var v: Chase.Vehicle = chase.vehicles[0]
	var p := World.p3(v.x, v.y)
	var a := float(v.a) / Trig.TURN * TAU
	var fwd := Vector3(cos(a), 0, -sin(a))
	var tr: Truck = world.trucks[0]
	tr.position = p
	tr.rotation.y = a
	var mode: String = args[2] if args.size() > 2 else "chase"
	var target := Vector3.ZERO
	var best := 1e9
	for st in chase.storms:
		var sp := World.p3(st.x, st.y)
		if sp.distance_to(p) < best:
			best = sp.distance_to(p)
			target = sp
	var to := (target - p)
	to.y = 0
	to = to.normalized()
	if mode == "chase":
		cam.position = p - to * 16.0 + Vector3(0, 6, 0)
		cam.look_at(p + to * 40.0 + Vector3(0, 10, 0))
	else:
		cam.position = p + Vector3(0, 2.6, 0)
		cam.look_at(target + Vector3(0, 900, 0))
	world.frame(dt, cam.position)
	if frames == 6:
		root.get_texture().get_image().save_png("/tmp/%s.png" % (args[1] if args.size() > 1 else "rw"))
		return true
	return false
