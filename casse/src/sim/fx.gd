class_name Fx
## Fixed-point maths: everything in the simulation is an integer, so a run is identical on every machine.
## Positions are in 1/256 of a tile; angles in 1/256 of a turn (0 = east, 64 = south: y grows downwards);
## trigonometry comes from a frozen table scaled by 1024.

const ONE := 256
const COS: PackedInt32Array = [1024,1024,1023,1021,1019,1016,1013,1009,1004,999,993,987,980,972,964,955,946,936,926,915,903,891,878,865,851,837,822,807,792,775,759,742,724,706,688,669,650,630,610,590,569,548,526,505,483,460,438,415,392,369,345,321,297,273,249,224,200,175,150,125,100,75,50,25,0,-25,-50,-75,-100,-125,-150,-175,-200,-224,-249,-273,-297,-321,-345,-369,-392,-415,-438,-460,-483,-505,-526,-548,-569,-590,-610,-630,-650,-669,-688,-706,-724,-742,-759,-775,-792,-807,-822,-837,-851,-865,-878,-891,-903,-915,-926,-936,-946,-955,-964,-972,-980,-987,-993,-999,-1004,-1009,-1013,-1016,-1019,-1021,-1023,-1024,-1024,-1024,-1023,-1021,-1019,-1016,-1013,-1009,-1004,-999,-993,-987,-980,-972,-964,-955,-946,-936,-926,-915,-903,-891,-878,-865,-851,-837,-822,-807,-792,-775,-759,-742,-724,-706,-688,-669,-650,-630,-610,-590,-569,-548,-526,-505,-483,-460,-438,-415,-392,-369,-345,-321,-297,-273,-249,-224,-200,-175,-150,-125,-100,-75,-50,-25,0,25,50,75,100,125,150,175,200,224,249,273,297,321,345,369,392,415,438,460,483,505,526,548,569,590,610,630,650,669,688,706,724,742,759,775,792,807,822,837,851,865,878,891,903,915,926,936,946,955,964,972,980,987,993,999,1004,1009,1013,1016,1019,1021,1023,1024]

static func cosa(a: int) -> int:
	return COS[a & 255]

static func sina(a: int) -> int:
	return COS[(a - 64) & 255]

## Integer square root (floor).
static func isqrt(n: int) -> int:
	if n <= 0:
		return 0
	var x := int(sqrt(float(n)))
	while x * x > n:
		x -= 1
	while (x + 1) * (x + 1) <= n:
		x += 1
	return x

## Angle of a vector (0..255), by binary search on the table: exact and deterministic.
static func angle(dx: int, dy: int) -> int:
	if dx == 0 and dy == 0:
		return 0
	var best := 0
	var best_dot := -9223372036854775807
	# Coarse pass every 8 steps, then refine around the best.
	for a in range(0, 256, 8):
		var d := dx * COS[a] + dy * COS[(a - 64) & 255]
		if d > best_dot:
			best_dot = d
			best = a
	var c := best
	for a in range(c - 7, c + 8):
		var d := dx * COS[a & 255] + dy * COS[(a - 64) & 255]
		if d > best_dot:
			best_dot = d
			best = a & 255
	return best

## Signed shortest difference b - a in 1/256 turns (-128..127).
static func adiff(a: int, b: int) -> int:
	return ((b - a + 128) & 255) - 128

## The eight move directions (input index 1..8): E, SE, S, SW, W, NW, N, NE.
const DIR_X: PackedInt32Array = [0, 1, 1, 0, -1, -1, -1, 0, 1]
const DIR_Y: PackedInt32Array = [0, 0, 1, 1, 1, 0, -1, -1, -1]
const DIR_A: PackedInt32Array = [0, 0, 32, 64, 96, 128, 160, 192, 224]
