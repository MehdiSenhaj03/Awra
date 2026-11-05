extends Node
## Audio (autoload "Sound"). The ball's waltz (one arrangement per venue) plays with a tension stem cut to the same
## length and kept in sync; the stem rises as the net tightens. The crowd murmurs underneath. A music-box waltz on
## the menus.

var vol_music := 0.7
var vol_sfx := 0.8
var _fx := AudioStreamPlayer.new()
var _fx_pb: AudioStreamPlaybackPolyphonic
var _cache: Dictionary = {}
var _last: Dictionary = {}
var _title := AudioStreamPlayer.new()
var _waltz := AudioStreamPlayer.new()
var _tension := AudioStreamPlayer.new()
var _crowd := AudioStreamPlayer.new()
var _mode := ""
var _title_gain := 0.0
var _ball_gain := 0.0
var _tension_gain := 0.0
var _tension_target := 0.0
var _lowpass: AudioEffectLowPassFilter
var _cut := 20000.0
var muffled := false

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	var comp := AudioEffectCompressor.new()
	comp.threshold = -8.0
	comp.ratio = 5.0
	AudioServer.add_bus_effect(0, comp)
	AudioServer.add_bus(1)
	AudioServer.set_bus_name(1, "Music")
	AudioServer.set_bus_send(1, "Master")
	_lowpass = AudioEffectLowPassFilter.new()
	_lowpass.cutoff_hz = 20000.0
	AudioServer.add_bus_effect(1, _lowpass)
	AudioServer.add_bus(2)
	AudioServer.set_bus_name(2, "Sfx")
	AudioServer.set_bus_send(2, "Master")
	set_volumes(vol_music, vol_sfx)
	_fx.bus = "Sfx"
	var poly := AudioStreamPolyphonic.new()
	poly.polyphony = 32
	_fx.stream = poly
	add_child(_fx)
	_fx.play()
	_fx_pb = _fx.get_stream_playback()
	for p: AudioStreamPlayer in [_title, _waltz, _tension, _crowd]:
		p.bus = "Music"
		p.volume_db = -80.0
		add_child(p)
	_title.stream = _loop("title")
	_tension.stream = _loop("tension")
	_crowd.stream = _loop("crowd")
	_crowd.bus = "Sfx"

func _loop(n: String) -> AudioStream:
	var s := _stream("res://assets/audio/music/%s.ogg" % n) as AudioStreamOggVorbis
	if s:
		s.loop = true
	return s

func set_volumes(music: float, sfx_v: float) -> void:
	vol_music = music
	vol_sfx = sfx_v
	AudioServer.set_bus_volume_db(1, linear_to_db(maxf(0.0001, music)))
	AudioServer.set_bus_volume_db(2, linear_to_db(maxf(0.0001, sfx_v)))

func _stream(path: String) -> AudioStream:
	var s: AudioStream = _cache.get(path)
	if s == null and ResourceLoader.exists(path):
		s = load(path)
		_cache[path] = s
	return s

func sfx(name: String, pitch: float = 1.0, vol: float = 1.0) -> void:
	var now := Time.get_ticks_msec() / 1000.0
	if _last.has(name) and now - float(_last[name]) < 0.04:
		return
	_last[name] = now
	var s := _stream("res://assets/audio/sfx/%s.ogg" % name)
	if s:
		_fx_pb.play_stream(s, 0.0, linear_to_db(maxf(0.0001, vol)), pitch)

## "title" or "ball_<venue>".
func music(mode: String) -> void:
	if mode == _mode:
		return
	_mode = mode
	if mode == "title":
		if not _title.playing and _title.stream:
			_title.play()
	elif mode.begins_with("ball_"):
		var w := _loop(mode.substr(5))
		if w:
			_waltz.stream = w
			_waltz.play()
			if _tension.stream:
				_tension.play()
			if _crowd.stream and not _crowd.playing:
				_crowd.play()
		_tension_target = 0.0

## 0..1: how close the end is (missions done, time running out, the laser on the spy).
func tension(x: float) -> void:
	_tension_target = clampf(x, 0.0, 1.0)

func _process(dt: float) -> void:
	var k := 1.0 - exp(-dt * 2.0)
	var ball := _mode.begins_with("ball_")
	_title_gain += ((1.0 if _mode == "title" else 0.0) - _title_gain) * k
	_ball_gain += ((1.0 if ball else 0.0) - _ball_gain) * k
	_tension_gain += (_tension_target - _tension_gain) * (1.0 - exp(-dt * 1.2))
	_title.volume_db = linear_to_db(maxf(0.0001, _title_gain))
	_waltz.volume_db = linear_to_db(maxf(0.0001, _ball_gain * (1.0 - 0.35 * _tension_gain)))
	_tension.volume_db = linear_to_db(maxf(0.0001, _ball_gain * _tension_gain))
	_crowd.volume_db = linear_to_db(maxf(0.0001, _ball_gain * 0.9))
	if _title_gain < 0.002 and _title.playing:
		_title.stop()
	if _ball_gain < 0.002 and not ball:
		for p: AudioStreamPlayer in [_waltz, _tension, _crowd]:
			if p.playing:
				p.stop()
	# Keep the tension stem on the waltz's beat.
	if _waltz.playing and _tension.playing:
		var a := _waltz.get_playback_position()
		var b := _tension.get_playback_position()
		var length := _waltz.stream.get_length()
		var d := absf(a - b)
		if length > 0.0 and minf(d, length - d) > 0.06 and absf(length - _tension.stream.get_length()) < 0.05:
			_tension.seek(a)
	_cut += ((900.0 if muffled else 20000.0) - _cut) * (1.0 - exp(-dt * 5.0))
	_lowpass.cutoff_hz = _cut
