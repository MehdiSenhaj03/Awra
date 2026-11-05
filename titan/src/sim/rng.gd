class_name Rng
extends RefCounted
## Deterministic integer randomness (xorshift32).

var s := 1

func _init(seed_v: int = 1) -> void:
	s = (seed_v & 0x7fffffff) | 1

func next() -> int:
	s ^= (s << 13) & 0xffffffff
	s ^= s >> 17
	s ^= (s << 5) & 0xffffffff
	s &= 0xffffffff
	return s

## Integer in [a, b].
func rint(a: int, b: int) -> int:
	return a + next() % (b - a + 1)

func chance(per1000: int) -> bool:
	return next() % 1000 < per1000
