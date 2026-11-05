class_name Venue
## A ballroom: tiles, furniture, points of interest (where guests go and what they do there), mission spots,
## and a distance field to every point of interest (guests walk down it).
##
## Map characters: '#' wall, '.' floor, ',' dance floor, 'O' column, 'T' table, 'S' statue, 'F' fountain,
## 'H' sofa, 'W' writing desk — all but floor block walking.
## Points of interest on floor tiles: 'b' buffet, 'p' painting, 's' statue, 'c' conversation, 'D' dance centre,
## 'h' sofa seat, 'n' window, 'e' entrance; mission spots: 'l' letter (at the desk), 'x' poison (at the buffet),
## 'u' bug (at a statue).

const WALK := 0
const BLOCK := 1

var data: Dictionary
var w := 0
var h := 0
var tiles := PackedByteArray()      # 0 floor, 1 dance floor, 2 wall, 3 column, 4 table, 5 statue, 6 fountain, 7 sofa, 8 desk
var pois: Array[Dictionary] = []    # {kind, x, y, cap, face, field: PackedInt32Array}
var entrances: Array[int] = []
var missions: Dictionary = {}       # kind -> poi index

func _init(d: Dictionary) -> void:
	data = d
	var rows: Array = d.rows
	h = rows.size()
	w = 0
	for r: String in rows:
		w = maxi(w, r.length())
	tiles.resize(w * h)
	for y in h:
		var r: String = rows[y]
		for x in w:
			var c := r[x] if x < r.length() else "#"
			var t := 0
			match c:
				"#": t = 2
				",": t = 1
				"O": t = 3
				"T": t = 4
				"S": t = 5
				"F": t = 6
				"H": t = 7
				"W": t = 8
			tiles[y * w + x] = t
			var kind := ""
			match c:
				"b": kind = "buffet"
				"p": kind = "painting"
				"s": kind = "statue"
				"c": kind = "talk"
				"D": kind = "dance"
				"h": kind = "seat"
				"n": kind = "window"
				"e": kind = "entrance"
				"l": kind = "letter"
				"x": kind = "poison"
				"u": kind = "bug"
			if kind != "":
				var cap: int = {"talk": 6, "dance": 12, "buffet": 3, "painting": 2, "statue": 2, "window": 2, "entrance": 99, "seat": 1}.get(kind, 1)
				var p := {"kind": kind, "x": x, "y": y, "cap": cap, "face": 0}
				if kind in ["letter", "poison", "bug"]:
					missions[kind] = pois.size()
				if kind == "entrance":
					entrances.append(pois.size())
				pois.append(p)
	for p in pois:
		p.face = _face_for(p)
		p.field = _field(p.x, p.y)

func walkable(x: int, y: int) -> bool:
	if x < 0 or y < 0 or x >= w or y >= h:
		return false
	return tiles[y * w + x] <= 1

func tile(x: int, y: int) -> int:
	if x < 0 or y < 0 or x >= w or y >= h:
		return 2
	return tiles[y * w + x]

## Guests at a buffet, painting, statue, desk face the furniture or wall next to them.
func _face_for(p: Dictionary) -> int:
	var want: Array = {"buffet": [4], "poison": [4], "painting": [2], "window": [2], "statue": [5, 6], "bug": [5], "letter": [8], "seat": [7]}.get(p.kind, [])
	var dirs := [[1, 0, 0], [0, 1, 64], [-1, 0, 128], [0, -1, 192]]
	for d in dirs:
		if want.has(tile(p.x + d[0], p.y + d[1])):
			return d[2]
	return 64

## Steps (×10, diagonals 14) from every walkable tile to the point.
func _field(gx: int, gy: int) -> PackedInt32Array:
	var f := PackedInt32Array()
	f.resize(w * h)
	f.fill(1 << 29)
	f[gy * w + gx] = 0
	var q := PackedInt32Array([gy * w + gx])
	var head := 0
	while head < q.size():
		var c := q[head]
		head += 1
		var cx := c % w
		var cy := c / w
		for d in [[1, 0, 10], [-1, 0, 10], [0, 1, 10], [0, -1, 10], [1, 1, 14], [1, -1, 14], [-1, 1, 14], [-1, -1, 14]]:
			var nx: int = cx + d[0]
			var ny: int = cy + d[1]
			if not walkable(nx, ny):
				continue
			if d[0] != 0 and d[1] != 0 and (not walkable(cx + d[0], cy) or not walkable(cx, cy + d[1])):
				continue
			var nd: int = f[c] + d[2]
			var n := ny * w + nx
			if nd < f[n]:
				f[n] = nd
				q.append(n)
	return f
