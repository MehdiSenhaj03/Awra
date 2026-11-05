class_name Level
## A vault: tiles, devices, guards, loop length and echo budget. Serialises to a short shareable code.
##
## Tiles: '.' floor, '#' wall, 'g' glass (noisy), 'E' exit, ' ' void (outside the building).
## Objects ("objs"), all on tile coordinates:
##   door    {k: "door" | "chan" | "key" | "vault" | "time", ch, color, inv, t1, t2}
##   plate   {ch}            pressed while someone stands on it
##   button  {ch, dur}       pulse
##   lever   {ch}            toggle
##   vlever  {ch}            spring lever of a vault door (all its levers at once)
##   term    {ch, dur}       hack (hold), then pulse
##   laser   {ch, dir: 0 horizontal | 1 vertical, len}   alarm when crossed, off while its channel is on
##   cam     {ch, a0, a1, range, speed}                   sweeping camera, off while its channel is on
##   dark    {w, h, ch, inv} a zone without light (guards see 2 tiles), lit while its channel is on (inv: reversed)
##   card    {color}
##   safe    {}
##   gem     {}              optional jewels for the perfect run
## Guards: {path: [[x, y], ...], wait, range, dir}

const CODE_PREFIX := "CASSE1:"

var data: Dictionary = {}

func _init(d: Dictionary = {}) -> void:
	data = d if not d.is_empty() else blank(16, 10)

static func blank(w: int, h: int) -> Dictionary:
	var rows: Array[String] = []
	for y in h:
		var r := ""
		for x in w:
			r += "#" if x == 0 or y == 0 or x == w - 1 or y == h - 1 else "."
		rows.append(r)
	return {"v": 1, "name": "", "author": "", "w": w, "h": h, "rows": rows, "loop": 2400, "budget": 4, "par": 0, "spawn": [1, h - 2], "objs": [], "guards": []}

var w: int:
	get: return int(data.w)
var h: int:
	get: return int(data.h)

func tile(x: int, y: int) -> String:
	if x < 0 or y < 0 or x >= w or y >= h:
		return " "
	return String(data.rows[y])[x]

func set_tile(x: int, y: int, c: String) -> void:
	var r: String = data.rows[y]
	data.rows[y] = r.substr(0, x) + c + r.substr(x + 1)

static func encode(d: Dictionary) -> String:
	var raw := JSON.stringify(d).to_utf8_buffer()
	var z := raw.compress(FileAccess.COMPRESSION_DEFLATE)
	var b := PackedByteArray()
	b.append(raw.size() & 255)
	b.append((raw.size() >> 8) & 255)
	b.append((raw.size() >> 16) & 255)
	b.append_array(z)
	return CODE_PREFIX + Marshalls.raw_to_base64(b).replace("+", "-").replace("/", "_").replace("=", "")

static func decode(code: String) -> Dictionary:
	code = code.strip_edges().replace(" ", "").replace("\n", "")
	if not code.begins_with(CODE_PREFIX):
		return {}
	var s := code.substr(CODE_PREFIX.length()).replace("-", "+").replace("_", "/")
	while s.length() % 4 != 0:
		s += "="
	var b := Marshalls.base64_to_raw(s)
	if b.size() < 4:
		return {}
	var n := b[0] | (b[1] << 8) | (b[2] << 16)
	if n <= 0 or n > 1 << 20:
		return {}
	var raw := b.slice(3).decompress(n, FileAccess.COMPRESSION_DEFLATE)
	var v: Variant = JSON.parse_string(raw.get_string_from_utf8())
	if not (v is Dictionary) or not v.has("rows"):
		return {}
	return v
