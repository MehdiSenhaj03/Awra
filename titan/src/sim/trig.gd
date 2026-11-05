class_name Trig
## Integer trigonometry, bit-for-bit identical on every machine: angles in 1/4096 of a turn, sines and cosines in
## 1/65536, a table built by an integer rotation recurrence (no floating point), atan2 by integer CORDIC, isqrt.

const TURN := 4096
const ONE := 65536
static var _cos := PackedInt32Array()
static var _sin := PackedInt32Array()

static func _build() -> void:
	if not _cos.is_empty():
		return
	_cos.resize(TURN)
	_sin.resize(TURN)
	# Rotation by one step of 2π/4096 in 2^30 fixed point: cos = 1073740589, sin = 1647099.
	const CD := 1073740589
	const SD := 1647099
	var c := 1 << 30
	var s := 0
	for k in TURN / 4 + 1:
		var cc := (c + (1 << 13)) >> 14
		var ss := (s + (1 << 13)) >> 14
		if k <= TURN / 4:
			# Fill the four quadrants by symmetry.
			for q in 4:
				var idx := (k + q * TURN / 4) % TURN
				match q:
					0:
						_cos[idx] = cc
						_sin[idx] = ss
					1:
						_cos[idx] = -ss
						_sin[idx] = cc
					2:
						_cos[idx] = -cc
						_sin[idx] = -ss
					3:
						_cos[idx] = ss
						_sin[idx] = -cc
		var nc := (c * CD - s * SD) >> 30
		var ns := (s * CD + c * SD) >> 30
		c = nc
		s = ns

static func cosa(a: int) -> int:
	_build()
	return _cos[a & (TURN - 1)]

static func sina(a: int) -> int:
	_build()
	return _sin[a & (TURN - 1)]

## Rotates (x, y) by angle a (1/4096 turns).
static func rot(x: int, y: int, a: int) -> Vector2i:
	var c := cosa(a)
	var s := sina(a)
	return Vector2i((x * c - y * s) >> 16, (x * s + y * c) >> 16)

const _ATAN := [512, 302, 160, 81, 41, 20, 10, 5, 3, 1, 1]

## Angle of (x, y) in 1/4096 turns (0 = +x, 1024 = +y).
static func atan2i(y: int, x: int) -> int:
	if x == 0 and y == 0:
		return 0
	var a := 0
	# Bring the vector into the right half-plane.
	if x < 0:
		x = -x
		y = -y
		a = 2048
	# Scale down very long vectors (CORDIC keeps the angle).
	while absi(x) > (1 << 40) or absi(y) > (1 << 40):
		x >>= 1
		y >>= 1
	x <<= 8
	y <<= 8
	var acc := 0
	for i in _ATAN.size():
		var nx: int
		if y > 0:
			nx = x + (y >> i)
			y = y - (x >> i)
			acc += _ATAN[i]
		else:
			nx = x - (y >> i)
			y = y + (x >> i)
			acc -= _ATAN[i]
		x = nx
	return (a + acc) & (TURN - 1)

## Signed smallest difference b - a in 1/4096 turns.
static func adiff(a: int, b: int) -> int:
	return ((b - a + TURN / 2) & (TURN - 1)) - TURN / 2

static func isqrt(n: int) -> int:
	if n <= 0:
		return 0
	var x := n
	var y := (x + 1) >> 1
	while y < x:
		x = y
		y = (x + n / x) >> 1
	return x
