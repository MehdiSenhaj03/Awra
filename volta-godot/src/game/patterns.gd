class_name P
extends RefCounted
## Bullet pattern helpers and the palette of the Silence.

const MAGENTA := 0xff2fb9
const CYAN := 0x2ef2ff
const GOLD := 0xffc34a
const VIOLET := 0x9b5cff
const RED := 0xff4060
const WHITE := 0xfff4e8
const GREEN := 0x40ffb0
const ORANGE := 0xff7a2f

## n bullets spread over `spread` radians around angle a.
static func fan(w: World, x: float, y: float, a: float, n: int, spread: float, speed: float, o: Dictionary = {}) -> void:
	for i in n:
		var t := 0.0 if n == 1 else float(i) / float(n - 1) - 0.5
		w.bullets.add(x, y, a + t * spread, speed, o)

## A full ring of n bullets.
static func ring(w: World, x: float, y: float, n: int, speed: float, offset: float = 0.0, o: Dictionary = {}) -> void:
	for i in n:
		w.bullets.add(x, y, offset + (float(i) / float(n)) * U.TAU_F, speed, o)

## A column of bullets at increasing speeds.
static func column(w: World, x: float, y: float, a: float, n: int, speed: float, step: float, o: Dictionary = {}) -> void:
	for i in n:
		w.bullets.add(x, y, a, speed + float(i) * step, o)
