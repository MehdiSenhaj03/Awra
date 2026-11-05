extends SceneTree
## Lockstep check: run twice (-- host / -- join). The host plays the spy with the AI, the guest the sniper with the
## AI (whose aim/fire travel as inputs); both simulations must end identical.
var net: Online
var role := ""
var sim: Sim
var spy: SpyAI
var hunter: HunterAI
var mock: Mock
var frames := 0

class Mock extends RefCounted:
	var t: SceneTree
	func local_net_input() -> Array:
		var s: SceneTree = t
		if s.role == "host":
			return [s.spy.input(), 0]
		var h: Array = s.hunter.update()
		var a: Vector2i = h[0]
		return [(clampi(a.x, 0, 65535) << 16) | clampi(a.y, 0, 65535), 1 if h[1] else 0]
	func step_net(host_in: Array, guest_in: Array) -> void:
		var s: SceneTree = t
		var packed := int(guest_in[0])
		s.sim.step(PackedByteArray([int(host_in[0]) & 255]), Vector2i(packed >> 16, packed & 65535), int(guest_in[1]) != 0)

func _init() -> void:
	role = OS.get_cmdline_user_args()[0]
	net = Online.new()
	net.name = "Online"
	root.add_child(net)
	var vd: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://venues/salon.json"))
	sim = Sim.new(Venue.new(vd), "contract", 4242, int(vd.guests))
	# Each side only runs its own AI (the other side's choices arrive over the network).
	spy = SpyAI.new(sim, 1.0, 11)
	hunter = HunterAI.new(sim, 1.0, 12)
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
	if sim.over != "" or frames > 40000:
		var h := 0
		for g in sim.guests:
			h = (h * 31 + g.x * 7 + g.y) % 1000000007
		print(role, " over=", sim.over, " frame=", sim.frame, " hash=", h)
		quit()
