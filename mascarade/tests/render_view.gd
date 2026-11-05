extends SceneTree
## Renders a venue with its crowd and saves screenshots. -- <venue> <frames before shot> <out prefix> [zoom]
var view: BallView
var sim: Sim
var frames := 0
var shots := 0
var spy: SpyAI
var hunter: HunterAI
var args: PackedStringArray
var inp := PackedByteArray([0])

func _initialize() -> void:
	args = OS.get_cmdline_user_args()
	var vid: String = args[0] if args.size() > 0 else "salon"
	var vd: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://venues/%s.json" % vid))
	sim = Sim.new(Venue.new(vd), "contract", 1003, int(vd.guests))
	view = BallView.new(sim, vid)
	root.add_child(view)
	view.set_quality(true, true, false)
	spy = SpyAI.new(sim, 1.0, 9)
	hunter = HunterAI.new(sim, 1.0, 5)
	view.mark_guest = sim.spy
	if args.size() > 3:
		view.zoom = float(args[3])
	# Warm the crowd up a few seconds.
	for i in 600:
		inp[0] = spy.input()
		var h: Array = hunter.update()
		sim.step(inp, h[0], false)
	view.crowd.snap()
	view.crowd.snap()

func _process(dt: float) -> bool:
	for k in 2:
		inp[0] = spy.input()
		var h: Array = hunter.update()
		sim.step(inp, h[0], false)
		view.crowd.snap()
		view.set_aim(Vector2(h[0].x / 256.0, h[0].y / 256.0))
	view.crowd.draw(1.0, 1.0 / 30.0)
	if args.size() > 4:
		view.set_aim(Vector2(float(args[4]), float(args[5])))
	frames += 1
	var wait := int(args[1]) if args.size() > 1 else 8
	if frames == wait:
		var img := root.get_texture().get_image()
		img.save_png("/tmp/%s_%d.png" % [args[2] if args.size() > 2 else "rv", shots])
		shots += 1
		return true
	return false
