class_name Rng
extends RefCounted
## Deterministic PRNG (mulberry32), bit-identical to the TypeScript version.

var s: int

func _init(seed_value: int = 1) -> void:
	s = seed_value & 0xFFFFFFFF

static func imul(a: int, b: int) -> int:
	return (a * b) & 0xFFFFFFFF

func next() -> float:
	s = (s + 0x6d2b79f5) & 0xFFFFFFFF
	var t: int = s
	t = imul(t ^ (t >> 15), t | 1)
	t = (t ^ ((t + imul(t ^ (t >> 7), t | 61)) & 0xFFFFFFFF)) & 0xFFFFFFFF
	return float((t ^ (t >> 14)) & 0xFFFFFFFF) / 4294967296.0

func range_f(a: float, b: float) -> float:
	return a + (b - a) * next()

func int_r(a: int, b: int) -> int:
	return int(floorf(range_f(a, b + 1)))

func chance(p: float) -> bool:
	return next() < p

func pick(arr: Array) -> Variant:
	return arr[int(floorf(next() * arr.size()))]

func sign() -> float:
	return -1.0 if next() < 0.5 else 1.0
