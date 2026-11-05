extends SceneTree
## Lockstep check: run twice (-- host / -- join). Each side's mage is played by an AI on its own machine; only the
## encoded casts travel. Both duels must end identical.
var net: Online
var role := ""
var duel: Duel
var ai: DuelAI
var mock: Mock
var frames := 0

class Mock extends RefCounted:
	var t: SceneTree
	func local_net_input() -> Array:
		var s: SceneTree = t
		return [CastCode.encode(s.ai.update()), 0]
	func step_net(host_in: Array, guest_in: Array) -> void:
		var s: SceneTree = t
		if s.duel.round_over >= 0 and s.duel.over < 0:
			s.duel.next_round()
		s.duel.step([CastCode.decode(int(host_in[0])), CastCode.decode(int(guest_in[0]))])

func _init() -> void:
	role = OS.get_cmdline_user_args()[0]
	net = Online.new()
	net.name = "Online"
	root.add_child(net)
	duel = Duel.new()
	ai = DuelAI.new(duel, 0 if role == "host" else 1, 1.0, 5 if role == "host" else 9)
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
	if duel.over >= 0 or frames > 60000:
		print(role, " over=", duel.over, " frame=", duel.frame, " hp=", duel.mages[0].hp, "/", duel.mages[1].hp, " casts=", duel.mages[0].casts, "/", duel.mages[1].casts)
		quit()
