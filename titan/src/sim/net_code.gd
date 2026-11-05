class_name NetCode
## A player's input as two ints for the network lockstep: the crew member, movement, post, the pilot's steering,
## pace, verb and anchor, the gunner's fire, the mechanic's stoke and vent, in the first; the harpoon's aim point
## (metres, 24 bits) in the second.

const VERBS := ["", "strike", "parry", "grab"]

static func pack(d: Dictionary) -> Array:
	var mv: Vector2i = d.get("move", Vector2i.ZERO)
	var a := (int(d.get("crew", -1)) + 1) & 7
	a |= (clampi(mv.x, -1, 1) + 1) << 3
	a |= (clampi(mv.y, -1, 1) + 1) << 5
	a |= (1 if d.get("use", false) else 0) << 7
	a |= (clampi(int(d.get("take", -1)), -1, 4) + 1) << 8
	a |= (clampi(int(d.get("steer", 0)), -8, 8) + 8) << 11
	a |= (clampi(int(d.get("pace", 0)), -1, 1) + 1) << 16
	a |= maxi(0, VERBS.find(String(d.get("verb", "")))) << 18
	a |= (1 if d.get("anchor", false) else 0) << 20
	a |= (1 if d.get("fire", false) else 0) << 21
	a |= (1 if d.get("stoke", false) else 0) << 22
	a |= (1 if d.get("vent", false) else 0) << 23
	a |= (1 if int(d.get("titan", 0)) == 1 else 0) << 24
	a |= (1 if d.has("aim") else 0) << 25
	var b := 0
	if d.has("aim"):
		var v: Vector3i = d.aim
		b = (clampi(v.x / 100 + 128, 0, 255)) | (clampi(v.y / 100 + 128, 0, 255) << 8) | (clampi(v.z / 50, 0, 255) << 16)
	return [a, b]

static func unpack(a: int, b: int) -> Dictionary:
	var d := {
		"crew": (a & 7) - 1,
		"move": Vector2i(((a >> 3) & 3) - 1, ((a >> 5) & 3) - 1),
		"use": ((a >> 7) & 1) == 1,
		"take": ((a >> 8) & 7) - 1,
		"steer": ((a >> 11) & 31) - 8,
		"pace": ((a >> 16) & 3) - 1,
		"verb": VERBS[(a >> 18) & 3],
		"anchor": ((a >> 20) & 1) == 1,
		"fire": ((a >> 21) & 1) == 1,
		"stoke": ((a >> 22) & 1) == 1,
		"vent": ((a >> 23) & 1) == 1,
		"titan": (a >> 24) & 1,
	}
	if ((a >> 25) & 1) == 1:
		d.aim = Vector3i(((b & 255) - 128) * 100, (((b >> 8) & 255) - 128) * 100, ((b >> 16) & 255) * 50)
	return d

## The same quantisation applied locally, so a local game plays exactly like its online twin.
static func normalize(d: Dictionary) -> Dictionary:
	var p := pack(d)
	return unpack(p[0], p[1])
