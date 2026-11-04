class_name EnemyBullets
extends RefCounted
## Enemy bullets as preallocated flat arrays (never resized: loops may hold local references safely).
## Speeds in units per second, life in seconds. flags: 1 = grazed, 2 = reflected (now hurts foes).

const ORB := 0
const SHARD := 1
const BIG := 2
const NOTE := 3

var max_n: int
var n: int = 0
var x := PackedFloat32Array()
var y := PackedFloat32Array()
var vx := PackedFloat32Array()
var vy := PackedFloat32Array()
var r := PackedFloat32Array()
var life := PackedFloat32Array()
var acc := PackedFloat32Array()
var turn := PackedFloat32Array()
var wait := PackedFloat32Array()
var dmg := PackedFloat32Array()
var color := PackedInt32Array()
var kind := PackedByteArray()
var flags := PackedByteArray()

func _init(m: int = 4000) -> void:
	max_n = m
	x.resize(m); y.resize(m); vx.resize(m); vy.resize(m); r.resize(m); life.resize(m)
	acc.resize(m); turn.resize(m); wait.resize(m); dmg.resize(m); color.resize(m); kind.resize(m); flags.resize(m)

## o: optional "r", "color", "kind", "life", "acc", "turn", "wait".
func add(px: float, py: float, a: float, speed: float, o: Dictionary = {}) -> int:
	if n >= max_n:
		return -1
	var i := n
	n += 1
	x[i] = px
	y[i] = py
	vx[i] = cos(a) * speed
	vy[i] = sin(a) * speed
	r[i] = o.get("r", 0.32)
	life[i] = o.get("life", 8.0)
	acc[i] = o.get("acc", 0.0)
	turn[i] = o.get("turn", 0.0)
	wait[i] = o.get("wait", 0.0)
	color[i] = o.get("color", 0xff2fb9)
	kind[i] = o.get("kind", ORB)
	flags[i] = 0
	dmg[i] = 0.0
	return i

func remove(i: int) -> void:
	n -= 1
	var j := n
	if i == j:
		return
	x[i] = x[j]; y[i] = y[j]; vx[i] = vx[j]; vy[i] = vy[j]; r[i] = r[j]; life[i] = life[j]
	acc[i] = acc[j]; turn[i] = turn[j]; wait[i] = wait[j]; color[i] = color[j]; kind[i] = kind[j]
	flags[i] = flags[j]; dmg[i] = dmg[j]

func clear() -> void:
	n = 0
