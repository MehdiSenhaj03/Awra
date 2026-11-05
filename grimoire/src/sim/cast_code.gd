class_name CastCode
## Duel casts as single integers, for the network lockstep: 0 nothing, 1/2 step up/down, otherwise glyph, lane and
## power packed together.

static func encode(c: Dictionary) -> int:
	if c.is_empty():
		return 0
	if c.has("step"):
		return 1 if int(c.step) < 0 else 2
	return 10 + (int(c.g) + 1) * 100000 + int(c.lane) * 1000 + int(c.pow)

static func decode(v: int) -> Dictionary:
	if v == 0:
		return {}
	if v == 1:
		return {"step": -1}
	if v == 2:
		return {"step": 1}
	v -= 10
	return {"g": v / 100000 - 1, "lane": (v / 1000) % 100, "pow": v % 1000}
