extends SceneTree
## Lockstep check (race mode): run twice (-- host / -- join). Each machine's truck is driven by an AI there; only
## the packed inputs travel. Both chases must end identical.
var net: Online
var role := ""
var chase: Chase
var ai: ChaserAI
var mock: Mock
var frames := 0

class Mock extends RefCounted:
	var t: SceneTree
	func local_net_input() -> Array:
		var s: SceneTree = t
		return NetCode.pack(s.ai.update())
	func step_net(host_in: Array, guest_in: Array) -> void:
		var s: SceneTree = t
		s.chase.step([NetCode.unpack(0, int(host_in[0]), int(host_in[1])), NetCode.unpack(1, int(guest_in[0]), int(guest_in[1]))])

func _init() -> void:
	role = OS.get_cmdline_user_args()[0]
	net = Online.new()
	net.name = "Online"
	root.add_child(net)
	chase = Chase.new({"seed": 6, "minutes": 3, "storms": [{"at": 0, "x": 4_500_000, "y": 4_000_000, "sev": 3, "dir": 560, "speed": 14, "tornadoes": 1}]}, 2)
	ai = ChaserAI.new(chase, 0 if role == "host" else 1, 1.0, 5 if role == "host" else 8)
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
	for i in 8:
		net.net_step(mock)
	if chase.over or frames > 80000:
		var h := 0
		for v in chase.vehicles:
			h = (h * 31 + v.x * 7 + v.y + v.hp + v.photos * 1000) % 1000000007
		print(role, " over frame=", chase.frame, " hash=", h, " dossiers=", chase.dossier(0).total, "/", chase.dossier(1).total)
		quit()
