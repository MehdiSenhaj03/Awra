class_name Plains
extends RefCounted
## The plains, generated from a seed: a one-kilometre grid of section roads (paved highways, dirt roads that turn
## to mud in the rain, gaps), a meandering river with its bridges, towns at some crossroads, farms, and fields.
## Lengths in millimetres; integers only.

const SIZE := 12_000_000
const CELL := 1_000_000
const N := 12                    # crossroads per side (roads at k * CELL, k = 1..N-1)
const FIELD := 250_000           # field patch size
const FN := SIZE / FIELD         # 48 patches per side
enum S { FIELD, DIRT, PAVED, WATER, TOWN }
enum F { WHEAT, CORN, PASTURE, PLOWED, TREES }

var seed_v := 1
var rng: Rng
## vroad[k][j]: the north-south road at x = k*CELL between y = j*CELL and (j+1)*CELL: 0 none, 1 dirt, 2 paved.
var vroad: Array = []
var hroad: Array = []
var river: PackedInt64Array = PackedInt64Array()   # polyline x0,y0,x1,y1,...
var river_w := 34_000
var bridges: Array = []          # [{x, y, v: bool}] where a road crosses the river with a bridge
var towns: Array = []            # [{x, y, name_i, houses: [[x, y], ...]}]
var farms: Array = []            # [[x, y, kind], ...]
var fields := PackedByteArray()  # FN*FN field kinds
var wet := PackedInt32Array()    # FN*FN wetness 0..1000

func _init(s: int) -> void:
	seed_v = s
	rng = Rng.new(s * 7919 + 13)
	_roads()
	_river()
	_towns()
	_farms()
	fields.resize(FN * FN)
	wet.resize(FN * FN)
	for i in FN * FN:
		var r := rng.rint(0, 99)
		fields[i] = F.WHEAT if r < 34 else (F.CORN if r < 60 else (F.PASTURE if r < 82 else (F.PLOWED if r < 96 else F.TREES)))

func _roads() -> void:
	vroad.clear()
	hroad.clear()
	for k in N:
		var vr := PackedByteArray()
		var hr := PackedByteArray()
		vr.resize(N)
		hr.resize(N)
		for j in N:
			vr[j] = 0 if (k == 0 or rng.chance(140)) else 1
			hr[j] = 0 if (k == 0 or rng.chance(140)) else 1
		vroad.append(vr)
		hroad.append(hr)
	# Two highways each way, paved end to end.
	for n in 2:
		var kv := rng.rint(2, N - 2)
		var kh := rng.rint(2, N - 2)
		for j in N:
			vroad[kv][j] = 2
			hroad[kh][j] = 2

func _river() -> void:
	# From the west edge to the east edge, meandering.
	var y := rng.rint(3, 8) * CELL + rng.rint(0, CELL)
	var x := -200_000
	var pts: Array[int] = []
	var dir := 0
	while x < SIZE + 200_000:
		pts.append(x)
		pts.append(y)
		x += rng.rint(250_000, 450_000)
		dir = clampi(dir + rng.rint(-1, 1), -2, 2)
		y = clampi(y + dir * rng.rint(80_000, 160_000), 2 * CELL, SIZE - 2 * CELL)
	river = PackedInt64Array(pts)
	# Bridges where roads cross it: always on paved roads, two dirt roads out of three.
	for k in range(1, N):
		var rx := k * CELL
		var ry := _river_y_at(rx)
		var j := ry / CELL
		if j >= 0 and j < N and vroad[k][j] > 0:
			if vroad[k][j] == 2 or rng.chance(660):
				bridges.append({"x": rx, "y": ry, "v": true})
			else:
				vroad[k][j] = 0
	for k in range(1, N):
		# Horizontal roads run along the river rarely; where they cross it (river near y = k*CELL), handle it.
		var hy := k * CELL
		for j in N:
			if hroad[k][j] == 0:
				continue
			var x0 := j * CELL
			var x1 := (j + 1) * CELL
			var crosses := false
			var cx := x0
			for s in 11:
				var xx := x0 + (x1 - x0) * s / 10
				var ry := _river_y_at(xx)
				if absi(ry - hy) < river_w:
					crosses = true
					cx = xx
					break
			if crosses:
				if hroad[k][j] == 2 or rng.chance(500):
					bridges.append({"x": cx, "y": hy, "v": false})
				else:
					hroad[k][j] = 0

func _river_y_at(x: int) -> int:
	for i in range(0, river.size() - 2, 2):
		var x0 := river[i]
		var x1 := river[i + 2]
		if x >= x0 and x <= x1:
			return river[i + 1] + (river[i + 3] - river[i + 1]) * (x - x0) / maxi(1, x1 - x0)
	return -SIZE

func _towns() -> void:
	var n := rng.rint(3, 4)
	var tries := 0
	while towns.size() < n and tries < 200:
		tries += 1
		var tx := rng.rint(2, N - 2) * CELL
		var ty := rng.rint(2, N - 2) * CELL
		var ok := absi(_river_y_at(tx) - ty) > 400_000
		for t: Dictionary in towns:
			if absi(int(t.x) - tx) + absi(int(t.y) - ty) < 3 * CELL:
				ok = false
		if not ok:
			continue
		var houses: Array = []
		for h in rng.rint(14, 26):
			var ax := rng.rint(-3, 3)
			var ay := rng.rint(-3, 3)
			if ax == 0 or ay == 0:
				continue
			houses.append([tx + ax * 70_000 + rng.rint(-12_000, 12_000), ty + ay * 70_000 + rng.rint(-12_000, 12_000)])
		towns.append({"x": tx, "y": ty, "name_i": rng.rint(0, 15), "houses": houses})

func _farms() -> void:
	for i in 70:
		var k := rng.rint(1, N - 1)
		var along := rng.rint(200_000, SIZE - 200_000)
		var side := 1 if rng.chance(500) else -1
		var off := side * rng.rint(70_000, 160_000)
		var x: int
		var y: int
		if rng.chance(500):
			x = k * CELL + off
			y = along
		else:
			x = along
			y = k * CELL + off
		if absi(_river_y_at(x) - y) < 120_000 or town_near(x, y, 500_000) >= 0:
			continue
		farms.append([x, y, rng.rint(0, 3)])

func town_near(x: int, y: int, r: int) -> int:
	for i in towns.size():
		var t: Dictionary = towns[i]
		if absi(int(t.x) - x) < r and absi(int(t.y) - y) < r:
			return i
	return -1

func road_at(x: int, y: int) -> int:
	if x < 0 or y < 0 or x >= SIZE or y >= SIZE:
		return 0
	var best := 0
	var k := (x + CELL / 2) / CELL
	if k > 0 and k < N and absi(x - k * CELL) <= 7_000:
		best = maxi(best, int(vroad[k][mini(N - 1, y / CELL)]))
	k = (y + CELL / 2) / CELL
	if k > 0 and k < N and absi(y - k * CELL) <= 7_000:
		best = maxi(best, int(hroad[k][mini(N - 1, x / CELL)]))
	return best

func in_river(x: int, y: int) -> bool:
	var ry := _river_y_at(x)
	if absi(ry - y) > river_w / 2:
		return false
	for b: Dictionary in bridges:
		if absi(int(b.x) - x) < 14_000 and absi(int(b.y) - y) < river_w:
			return false
	return true

## What the wheels are on: S.PAVED, S.DIRT, S.FIELD, S.WATER, S.TOWN.
func surface(x: int, y: int) -> int:
	var r := road_at(x, y)
	if r == 2:
		return S.PAVED
	if in_river(x, y):
		return S.WATER
	if r == 1:
		return S.DIRT
	if town_near(x, y, 260_000) >= 0:
		return S.TOWN
	return S.FIELD

func field_at(x: int, y: int) -> int:
	var fx := clampi(x / FIELD, 0, FN - 1)
	var fy := clampi(y / FIELD, 0, FN - 1)
	return fields[fy * FN + fx]

func wet_at(x: int, y: int) -> int:
	var fx := clampi(x / FIELD, 0, FN - 1)
	var fy := clampi(y / FIELD, 0, FN - 1)
	return wet[fy * FN + fx]
