class_name Online
extends Node
## Network play without a server: one player hosts (port opened automatically by UPnP when the router allows
## it), the other joins by address. A contract runs in lockstep: only inputs travel, the deterministic simulation
## does the rest.

signal connected
signal disconnected
signal message(kind: String, data: Variant)

const PORT := 47644
const DELAY := 4

var peer: ENetMultiplayerPeer
var is_host := false
var upnp_status := ""
var external := ""
# Lockstep.
var _local: Dictionary = {}
var _remote: Dictionary = {}
var _send_k := 0
var _exec_k := 0
var stall := 0

func host() -> bool:
	peer = ENetMultiplayerPeer.new()
	if peer.create_server(PORT, 1) != OK:
		return false
	multiplayer.multiplayer_peer = peer
	is_host = true
	multiplayer.peer_connected.connect(func(_id: int) -> void: connected.emit())
	multiplayer.peer_disconnected.connect(func(_id: int) -> void: disconnected.emit())
	_upnp.call_deferred()
	return true

func _upnp() -> void:
	var u := UPNP.new()
	var r := u.discover(1500, 2, "InternetGatewayDevice")
	if r == UPNP.UPNP_RESULT_SUCCESS and u.get_gateway() and u.get_gateway().is_valid_gateway():
		var m := u.add_port_mapping(PORT, PORT, "TITAN", "UDP")
		external = u.query_external_address()
		upnp_status = "ok" if m == UPNP.UPNP_RESULT_SUCCESS else "partial"
	else:
		upnp_status = "none"
	message.emit("upnp", upnp_status)

func local_addresses() -> String:
	var out: Array[String] = []
	for a in IP.get_local_addresses():
		if a.count(".") == 3 and not a.begins_with("127.") and not a.begins_with("169.254"):
			out.append(a)
	if external != "":
		out.push_front(external)
	return " / ".join(out)

func join(addr: String) -> bool:
	peer = ENetMultiplayerPeer.new()
	if peer.create_client(addr.strip_edges(), PORT) != OK:
		return false
	multiplayer.multiplayer_peer = peer
	is_host = false
	multiplayer.connected_to_server.connect(func() -> void: connected.emit())
	multiplayer.connection_failed.connect(func() -> void: disconnected.emit())
	multiplayer.server_disconnected.connect(func() -> void: disconnected.emit())
	return true

func close() -> void:
	if peer:
		peer.close()
	multiplayer.multiplayer_peer = null
	peer = null

func send(kind: String, data: Variant) -> void:
	if peer:
		_msg.rpc(kind, data)

@rpc("any_peer", "reliable")
func _msg(kind: String, data: Variant) -> void:
	message.emit(kind, data)

## The strokes being drawn right now, so the other player sees the glyph form (display only).
func send_strokes(pts: Array) -> void:
	if peer and peer.get_connection_status() == MultiplayerPeer.CONNECTION_CONNECTED:
		_strokes.rpc(pts)

@rpc("any_peer", "unreliable")
func _strokes(pts: Array) -> void:
	message.emit("strokes", pts)

# ---- Lockstep ------------------------------------------------------------------------------------------------------

func reset_lockstep() -> void:
	_local.clear()
	_remote.clear()
	for k in DELAY:
		_local[k] = [0, 0]
		_remote[k] = [0, 0]
	_send_k = DELAY
	_exec_k = 0
	stall = 0

@rpc("any_peer", "reliable")
func _in(k: int, b: int, f: int) -> void:
	_remote[k] = [b, f]

## Called every physics frame while an online match runs: sends this player's input DELAY frames ahead and
## advances the shared simulation when both players' inputs for the frame are known.
func net_step(heist: Object) -> void:
	if peer == null:
		return
	if _send_k - _exec_k < DELAY * 2:
		var mine: Array = heist.call("local_net_input")
		_local[_send_k] = mine
		_in.rpc(_send_k, mine[0], mine[1])
		_send_k += 1
	if not _remote.has(_exec_k) or not _local.has(_exec_k):
		stall += 1
		return
	stall = 0
	var a: Array = _local[_exec_k]
	var b: Array = _remote[_exec_k]
	_local.erase(_exec_k)
	_remote.erase(_exec_k)
	var host_in: Array = a if is_host else b
	var guest_in: Array = b if is_host else a
	heist.call("step_net", host_in, guest_in)
	_exec_k += 1
