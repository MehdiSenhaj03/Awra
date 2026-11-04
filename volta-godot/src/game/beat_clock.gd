class_name BeatClock
extends RefCounted
## The beat clock. It advances every simulation frame whatever happens (hit-stop and slow motion never bend
## it), so music and rhythm judgements always agree.

var frame: int = 0
var bpm: float
var _base: float = 0.0
var _base_frame: int = 0
var _prev_beat: float = 0.0
## Player calibration, in seconds.
var latency: float = 0.0
## Half-width of the PERFECT window (seconds).
var perfect_window: float = 0.07

func _init(b: float = 128.0) -> void:
	bpm = b

var beat: float:
	get: return _base + (float(frame - _base_frame) * U.DT * bpm) / 60.0

var spb: float:
	get: return 60.0 / bpm

func tick() -> void:
	_prev_beat = beat
	frame += 1

func set_bpm(b: float) -> void:
	_base = beat
	_base_frame = frame
	bpm = b

func crossed(div: float, offset: float = 0.0) -> bool:
	return floorf((beat - offset) * div) != floorf((_prev_beat - offset) * div)

func index(div: float) -> int:
	return int(floorf(beat * div))

func offset_sec(div: float = 1.0) -> float:
	var b := beat * div - (latency / spb) * div
	var f := b - U.jround(b)
	return (f * spb) / div

## "perfect", "great" or "off".
func judge(div: float = 1.0) -> String:
	var o := absf(offset_sec(div))
	if o <= perfect_window:
		return "perfect"
	if o <= perfect_window + 0.05:
		return "great"
	return "off"

func phase(div: float = 1.0) -> float:
	var b := beat * div
	return b - floorf(b)
