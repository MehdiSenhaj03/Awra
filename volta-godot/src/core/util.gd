class_name U
extends RefCounted
## Small math helpers matching the JavaScript semantics used by the original simulation.

const TAU_F := PI * 2.0
const DT := 1.0 / 60.0

## JavaScript Math.round: halves round towards +infinity.
static func jround(x: float) -> float:
	return floorf(x + 0.5)

static func angle_diff(a: float, b: float) -> float:
	var d := fmod(b - a, TAU_F)
	if d > PI:
		d -= TAU_F
	if d < -PI:
		d += TAU_F
	return d

## Frame-rate independent damping factor.
static func damp(k: float, dt: float = DT) -> float:
	return 1.0 - exp(-k * dt)

static func hypot(x: float, y: float) -> float:
	return sqrt(x * x + y * y)
