extends SceneTree
## Lockstep check (expedition together): run twice (-- host / -- join). The host plays the pilot, the guest a
## harpooner, each with local random inputs; only the packed inputs travel. Both legs must end identical.
var net: Online
var role := ""
var m: March
var mock: Mock
var frames := 0
var rng := RandomNumberGenerator.new()

class Mock extends RefCounted:
	var t: SceneTree
	func local_net_input() -> Array:
		var s: SceneTree = t
		return NetCode.pack(s.call("bot"))
	func step_net(host_in: Array, guest_in: Array) -> void:
		var s: SceneTree = t
		var mm: March = s.get("m")
		mm.step([NetCode.unpack(int(host_in[0]), int(host_in[1])), NetCode.unpack(int(guest_in[0]), int(guest_in[1]))])

func bot() -> Dictionary:
	if role == "host":
		var d := {"crew": 0, "steer": rng.randi_range(-8, 8), "anchor": rng.randf() < 0.1}
		if m.fight:
			d.verb = ["", "", "strike", "parry", "grab"][rng.randi_range(0, 4)]
		return d
	var t := Vector3i(rng.randi_range(-2000, 9000), rng.randi_range(-9000, 9000), rng.randi_range(0, 6000))
	if not m.birds.is_empty():
		var b: March.Bird = m.birds[0]
		t = Vector3i(b.x, b.y, b.z)
	return {"crew": 1, "aim": t, "fire": rng.randf() < 0.05, "take": 1 if frames < 5 else -1}

func _init() -> void:
	role = OS.get_cmdline_user_args()[0]
	rng.seed = 11 if role == "host" else 29
	net = Online.new()
	net.name = "Online"
	root.add_child(net)
	m = March.new({"kind": "ruins", "length_m": 900, "seed": 5, "level": 2, "boss": true, "foe_hp": 9000})
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
	if m.over != "" or m.frame > 60 * 400:
		var h := 0
		for c in m.crew:
			h = (h * 31 + c.x * 7 + c.y + c.hp) % 1000000007
		h = (h * 31 + m.titan.hp * 13 + m.progress + m.lateral + m.supplies + (m.foe.hp if m.foe else 0)) % 1000000007
		print(role, " over=", m.over, " frame=", m.frame, " hash=", h, " hp=", m.titan.hp, " foe=", m.foe.hp if m.foe else -1)
		quit()
