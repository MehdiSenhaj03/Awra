class_name NetCode
## A player's chase input as two ints for the network lockstep: driving bits and steering in the first, a photo's
## bearing, pitch and zoom (or a waypoint) in the second.

static func pack(d: Dictionary) -> Array:
	var a := (int(d.get("steer", 0)) + 16) | (32 if d.get("throttle", false) else 0) | (64 if d.get("brake", false) else 0) | (128 if d.get("horn", false) else 0) | (256 if d.get("probe", false) else 0)
	var b := 0
	if d.has("photo"):
		a |= 1024
		var ph: Dictionary = d.photo
		b = (int(ph.yaw) & 4095) | ((clampi(int(ph.pitch) + 2048, 0, 4095)) << 12) | (clampi(int(ph.zoom), 0, 1023) << 24)
	elif d.has("waypoint"):
		a |= 512
		var w: Vector2i = d.waypoint
		b = clampi(w.x / 1000, 0, 16383) | (clampi(w.y / 1000, 0, 16383) << 14)
	return [a, b]

static func unpack(t: int, a: int, b: int) -> Dictionary:
	var d := {"team": t, "steer": (a & 31) - 16, "throttle": (a & 32) != 0, "brake": (a & 64) != 0, "horn": (a & 128) != 0, "probe": (a & 256) != 0}
	if (a & 1024) != 0:
		d.photo = {"yaw": b & 4095, "pitch": ((b >> 12) & 4095) - 2048, "zoom": (b >> 24) & 1023}
	elif (a & 512) != 0:
		d.waypoint = Vector2i((b & 16383) * 1000, ((b >> 14) & 16383) * 1000)
	return d
