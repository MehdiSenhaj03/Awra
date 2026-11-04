class_name Shots
extends RefCounted
## The hero's shots (preallocated flat arrays).

const BOLT := 0
const ACCENT := 1
const PERFECT := 2
const PELLET := 3
const LANCE := 4
const MISSILE := 5
const ARC := 6

var max_n: int
var n: int = 0
var x := PackedFloat32Array()
var y := PackedFloat32Array()
var vx := PackedFloat32Array()
var vy := PackedFloat32Array()
var dmg := PackedFloat32Array()
var life := PackedFloat32Array()
var pierce := PackedByteArray()
var kind := PackedByteArray()
## Homing target id (missiles), 0 = none.
var target := PackedInt32Array()
## Last foe hit (pierce shots never hit the same foe twice in a row).
var last := PackedInt32Array()

func _init(m: int = 1500) -> void:
	max_n = m
	x.resize(m); y.resize(m); vx.resize(m); vy.resize(m); dmg.resize(m); life.resize(m)
	pierce.resize(m); kind.resize(m); target.resize(m); last.resize(m)

func add(px: float, py: float, a: float, speed: float, d: float, k: int, l: float = 0.9, p: int = 0) -> int:
	if n >= max_n:
		return -1
	var i := n
	n += 1
	x[i] = px
	y[i] = py
	vx[i] = cos(a) * speed
	vy[i] = sin(a) * speed
	dmg[i] = d
	life[i] = l
	pierce[i] = p
	kind[i] = k
	target[i] = 0
	last[i] = 0
	return i

func remove(i: int) -> void:
	n -= 1
	var j := n
	if i == j:
		return
	x[i] = x[j]; y[i] = y[j]; vx[i] = vx[j]; vy[i] = vy[j]; dmg[i] = dmg[j]; life[i] = life[j]
	pierce[i] = pierce[j]; kind[i] = kind[j]; target[i] = target[j]; last[i] = last[j]

func clear() -> void:
	n = 0
