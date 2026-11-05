extends SceneTree
## Lockstep check: run twice (-- host / -- join). Each peer drives its echo with a plan; both must end identical.
var net: Online
var role := ""
var run: Run
var planner: Planner
var mock: Mock
var frames := 0

class Mock extends RefCounted:
	var t: SceneTree
	func local_net_input() -> Array:
		var s: SceneTree = t
		return [s.planner.next(s.run.sim), 0]
	func step_net(bytes: Array, _flags: int) -> void:
		var s: SceneTree = t
		s.run.tick(bytes)

func _init() -> void:
	role = OS.get_cmdline_user_args()[0]
	net = Online.new()
	net.name = "Online"
	root.add_child(net)
	var lv := Level.new(JSON.parse_string(FileAccess.get_file_as_string("res://levels/campaign/1-2.json")))
	run = Run.new(lv, 2)
	var plans := [[["go", 3, 1], ["wait", 9999]], [["at", 20], ["go", 9, 2], ["hold", 95], ["go", 12, 6]]]
	planner = Planner.new(plans[0] if role == "host" else plans[1], 0 if role == "host" else 1)
	mock = Mock.new()
	mock.t = self
	net.connected.connect(func() -> void:
		print(role, " connected")
		net.reset_lockstep())
	process_frame.connect(_tick)

func _tick() -> void:
	frames += 1
	if frames == 2:
		if role == "host":
			net.host()
		else:
			net.join("127.0.0.1")
	if net.peer == null or net.peer.get_connection_status() != MultiplayerPeer.CONNECTION_CONNECTED:
		if frames > 3000:
			print(role, " TIMEOUT")
			quit(1)
		return
	if net._send_k == 0:
		return
	for i in 4:
		net.net_step(mock)
	if run.sim.over != "" or run.sim.frame >= run.loop_frames or frames > 20000:
		var h := 0
		for e in run.sim.echoes:
			h = h * 31 + e.x * 7 + e.y
		print(role, " over=", run.sim.over, " frame=", run.sim.frame, " hash=", h)
		quit()
