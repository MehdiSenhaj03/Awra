class_name Run
extends RefCounted
## A heist attempt across loops: the recordings of every echo, the live echo(es), rewinding, previews,
## paradox detection. The simulation itself is in Sim.

const TRACE_STEP := 6
const KEY_STEP := 60
const DESYNC := 48

var level: Level
var loop_frames := 2400
var budget := 4
## One recording per echo slot (input byte per frame) and the trajectory it produced when recorded.
var rec: Array[PackedByteArray] = []
var trace: Array[PackedInt32Array] = []
## Slots played live this loop (one in solo, two in coop).
var live: Array[int] = []
var sim: Sim
var keys: Dictionary = {}
var loops := 0
var alarms := 0

func _init(lv: Level, coop: int = 1) -> void:
	level = lv
	loop_frames = int(lv.data.get("loop", 2400))
	budget = int(lv.data.get("budget", 4))
	start_loop(coop)

## Starts a loop: the next free slots are played live (or the given slots are re-recorded).
func start_loop(n_live: int = 1, slots: Array = []) -> void:
	live.clear()
	if slots.is_empty():
		for k in n_live:
			if rec.size() < budget:
				rec.append(PackedByteArray())
				trace.append(PackedInt32Array())
				live.append(rec.size() - 1)
	else:
		for s: int in slots:
			live.append(s)
	for s in live:
		rec[s] = PackedByteArray()
		rec[s].resize(loop_frames)
		trace[s] = PackedInt32Array()
		trace[s].resize(loop_frames / TRACE_STEP * 2 + 2)
	sim = Sim.new(level)
	sim.reset_echoes(rec.size())
	keys.clear()
	keys[0] = sim.clone()
	loops += 1

func echo_count() -> int:
	return rec.size()

func is_live(slot: int) -> bool:
	return live.has(slot)

func _inputs(f: int, live_in: Array) -> PackedByteArray:
	var inp := PackedByteArray()
	inp.resize(rec.size())
	for s in rec.size():
		var li := live.find(s)
		if li >= 0 and li < live_in.size():
			inp[s] = live_in[li]
		elif f < rec[s].size():
			inp[s] = rec[s][f]
	return inp

## One frame with the live inputs (one byte per live slot).
func tick(live_in: Array) -> void:
	if sim.over != "" or sim.frame >= loop_frames:
		return
	var f := sim.frame
	for li in live.size():
		rec[live[li]][f] = live_in[li] if li < live_in.size() else 0
	sim.step(_inputs(f, live_in))
	var nf := sim.frame
	if nf % TRACE_STEP == 0:
		var ti := nf / TRACE_STEP * 2
		for s in rec.size():
			var e := sim.echoes[s]
			if live.has(s):
				if ti + 1 < trace[s].size():
					trace[s][ti] = e.x
					trace[s][ti + 1] = e.y
			elif not e.desync and ti + 1 < trace[s].size():
				if absi(trace[s][ti] - e.x) > DESYNC or absi(trace[s][ti + 1] - e.y) > DESYNC:
					e.desync = true
	if nf % KEY_STEP == 0:
		keys[nf] = sim.clone()

## Back to frame f of this loop: what the live echoes did after f is forgotten.
func rewind_to(f: int) -> void:
	f = clampi(f, 0, sim.frame)
	var k := (f / KEY_STEP) * KEY_STEP
	while not keys.has(k) and k > 0:
		k -= KEY_STEP
	sim = (keys[k] as Sim).clone()
	for kk in keys.keys():
		if kk > k:
			keys.erase(kk)
	while sim.frame < f:
		var live_in: Array = []
		for s in live:
			live_in.append(rec[s][sim.frame])
		tick(live_in)
	for s in live:
		for i in range(f, loop_frames):
			rec[s][i] = 0

## Frame f of this loop as it would unfold (live echoes stand still after the present). For the timeline.
func preview(f: int) -> Sim:
	var base := mini(f, sim.frame)
	var k := (base / KEY_STEP) * KEY_STEP
	while not keys.has(k) and k > 0:
		k -= KEY_STEP
	var s: Sim = (keys[k] as Sim).clone()
	while s.frame < f and s.over == "":
		var inp := PackedByteArray()
		inp.resize(rec.size())
		for slot in rec.size():
			if s.frame < sim.frame or not live.has(slot):
				inp[slot] = rec[slot][s.frame]
		s.step(inp)
	return s

## A finished heist to watch: every recording replays at once (no live echo).
static func from_replay(lv: Level, recs: Array) -> Run:
	var r := Run.new(lv, 0)
	r.rec.clear()
	r.trace.clear()
	for x in recs:
		r.rec.append(x)
		var t := PackedInt32Array()
		t.resize(r.loop_frames / TRACE_STEP * 2 + 2)
		r.trace.append(t)
	# Trajectories for the trails.
	var s := Sim.new(lv)
	s.reset_echoes(recs.size())
	var inp := PackedByteArray()
	inp.resize(recs.size())
	while s.frame < r.loop_frames and s.over == "":
		for k in recs.size():
			inp[k] = (recs[k] as PackedByteArray)[s.frame]
		s.step(inp)
		if s.frame % TRACE_STEP == 0:
			var ti := s.frame / TRACE_STEP * 2
			for k in recs.size():
				r.trace[k][ti] = s.echoes[k].x
				r.trace[k][ti + 1] = s.echoes[k].y
	r.live.clear()
	r.sim = Sim.new(lv)
	r.sim.reset_echoes(recs.size())
	r.keys.clear()
	r.keys[0] = r.sim.clone()
	return r

## The loop ran out: the live echoes become past echoes.
func loop_done() -> bool:
	return sim.frame >= loop_frames or sim.over != ""

func can_add_echo() -> bool:
	return rec.size() < budget

## Erases an echo entirely (later slots shift down).
func erase(slot: int) -> void:
	rec.remove_at(slot)
	trace.remove_at(slot)

## A compact replay of the whole heist: the level code plus every recording, run-length encoded.
func replay_code() -> String:
	var recs: Array = []
	for r in rec:
		recs.append(Marshalls.raw_to_base64(_rle(r)))
	var d := {"lv": Level.encode(level.data), "rec": recs}
	var raw := JSON.stringify(d).to_utf8_buffer()
	var z := raw.compress(FileAccess.COMPRESSION_DEFLATE)
	var b := PackedByteArray([raw.size() & 255, (raw.size() >> 8) & 255, (raw.size() >> 16) & 255])
	b.append_array(z)
	return "REPLAY1:" + Marshalls.raw_to_base64(b).replace("+", "-").replace("/", "_").replace("=", "")

static func parse_replay(code: String) -> Dictionary:
	code = code.strip_edges().replace(" ", "").replace("\n", "")
	if not code.begins_with("REPLAY1:"):
		return {}
	var s := code.substr(8).replace("-", "+").replace("_", "/")
	while s.length() % 4 != 0:
		s += "="
	var b := Marshalls.base64_to_raw(s)
	if b.size() < 4:
		return {}
	var n := b[0] | (b[1] << 8) | (b[2] << 16)
	var v: Variant = JSON.parse_string(b.slice(3).decompress(n, FileAccess.COMPRESSION_DEFLATE).get_string_from_utf8())
	if not (v is Dictionary):
		return {}
	var lv := Level.decode(v.lv)
	if lv.is_empty():
		return {}
	var recs: Array[PackedByteArray] = []
	for r: String in v.rec:
		recs.append(_unrle(Marshalls.base64_to_raw(r), int(lv.get("loop", 2400))))
	return {"level": lv, "rec": recs}

static func _rle(r: PackedByteArray) -> PackedByteArray:
	var out := PackedByteArray()
	var i := 0
	while i < r.size():
		var v := r[i]
		var n := 1
		while i + n < r.size() and r[i + n] == v and n < 255:
			n += 1
		out.append(n)
		out.append(v)
		i += n
	return out

static func _unrle(b: PackedByteArray, total: int) -> PackedByteArray:
	var out := PackedByteArray()
	var i := 0
	while i + 1 < b.size():
		for k in b[i]:
			out.append(b[i + 1])
		i += 2
	out.resize(total)
	return out

## Plays a full replay headless: the recordings in order, all at once. Returns the final sim.
static func simulate(lv: Level, recs: Array) -> Sim:
	var s := Sim.new(lv)
	s.reset_echoes(recs.size())
	var inp := PackedByteArray()
	inp.resize(recs.size())
	while s.frame < s.loop_frames and s.over == "":
		for k in recs.size():
			var r: PackedByteArray = recs[k]
			inp[k] = r[s.frame] if s.frame < r.size() else 0
		s.step(inp)
	return s
