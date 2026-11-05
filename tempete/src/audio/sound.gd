extends Node
## Audio (autoload "Sound"). The chase: the engine (pitch follows the speed), rain, hail and wind loops following
## the weather where the truck is, thunder that arrives late from far strikes (sound at 340 m/s), the music and its
## tension stem (rises as a tornado gets close). The menus have their own theme.

var vol_music := 0.7
var vol_sfx := 0.8
var muffled := false
var _fx := AudioStreamPlayer.new()
var _fx_pb: AudioStreamPlaybackPolyphonic
var _cache: Dictionary = {}
var _last: Dictionary = {}
var _title := AudioStreamPlayer.new()
var _music := AudioStreamPlayer.new()
var _tension := AudioStreamPlayer.new()
var _engine := AudioStreamPlayer.new()
var _rain := AudioStreamPlayer.new()
var _hail := AudioStreamPlayer.new()
var _wind := AudioStreamPlayer.new()
var _mode := ""
var _title_gain := 0.0
var _chase_gain := 0.0
var _tension_gain := 0.0
var _tension_target := 0.0
var _w := Vector4.ZERO            # engine, rain, hail, wind targets
var _wg := Vector4.ZERO
var _speed := 0.0
var _thunder: Array = []          # [{t, vol}]
var _lowpass: AudioEffectLowPassFilter
var _cut := 20000.0

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
	for p: AudioStreamPlayer in [_title, _music, _tension]:
		p.bus = "Music"
		p.volume_db = -80.0
		add_child(p)
	for p: AudioStreamPlayer in [_engine, _rain, _hail, _wind]:
		p.bus = "Sfx"
		p.volume_db = -80.0
		add_child(p)
	_title.stream = _loop("music/title")
	_music.stream = _loop("music/chase")
	_tension.stream = _loop("music/tension")
	_engine.stream = _loop("amb/engine")
	_rain.stream = _loop("amb/rain")
	_hail.stream = _loop("amb/hail")
	_wind.stream = _loop("amb/wind")

func _loop(n: String) -> AudioStream:
	var s := _stream("res://assets/audio/%s.ogg" % n) as AudioStreamOggVorbis
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
	if _last.has(name) and now - float(_last[name]) < 0.05:
		return
	_last[name] = now
	var s := _stream("res://assets/audio/sfx/%s.ogg" % name)
	if s:
		_fx_pb.play_stream(s, 0.0, linear_to_db(maxf(0.0001, vol)), pitch)

## A strike `dist` metres away: the thunder comes later and softer.
func thunder(dist: float) -> void:
	_thunder.append({"t": dist / 340.0, "vol": clampf(1.2 - dist / 9000.0, 0.15, 1.0), "near": dist < 1500.0})

## "title" or "chase".
func music(mode: String) -> void:
	if mode == _mode:
		return
	_mode = mode
	if mode == "title" and _title.stream and not _title.playing:
		_title.play()
	if mode == "chase":
		for p: AudioStreamPlayer in [_music, _tension, _engine, _rain, _hail, _wind]:
			if p.stream and not p.playing:
				p.play()
		_tension_target = 0.0

func tension(x: float) -> void:
	_tension_target = clampf(x, 0.0, 1.0)

func engine(speed_ms: float, alive: bool) -> void:
	_speed = speed_ms
	_w.x = 0.55 if alive else 0.0

func weather(rain: int, hail: int, wind: float) -> void:
	_w.y = clampf(rain / 80.0, 0.0, 1.0)
	_w.z = clampf(hail / 50.0, 0.0, 1.0)
	_w.w = clampf((wind - 6.0) / 40.0, 0.05, 1.0)

func _process(dt: float) -> void:
	var k := 1.0 - exp(-dt * 2.0)
	var chase := _mode == "chase"
	_title_gain += ((1.0 if _mode == "title" else 0.0) - _title_gain) * k
	_chase_gain += ((1.0 if chase else 0.0) - _chase_gain) * k
	_tension_gain += (_tension_target - _tension_gain) * (1.0 - exp(-dt * 1.0))
	_title.volume_db = linear_to_db(maxf(0.0001, _title_gain))
	_music.volume_db = linear_to_db(maxf(0.0001, _chase_gain * 0.7 * (1.0 - 0.4 * _tension_gain)))
	_tension.volume_db = linear_to_db(maxf(0.0001, _chase_gain * _tension_gain))
	var kk := 1.0 - exp(-dt * 3.0)
	var target := _w * _chase_gain
	_wg += (target - _wg) * kk
	_engine.volume_db = linear_to_db(maxf(0.0001, _wg.x))
	_engine.pitch_scale = clampf(0.7 + _speed / 30.0, 0.6, 2.2)
	_rain.volume_db = linear_to_db(maxf(0.0001, _wg.y))
	_hail.volume_db = linear_to_db(maxf(0.0001, _wg.z))
	_wind.volume_db = linear_to_db(maxf(0.0001, _wg.w))
	if _title_gain < 0.002 and _title.playing:
		_title.stop()
	if _chase_gain < 0.002 and not chase:
		for p: AudioStreamPlayer in [_music, _tension, _engine, _rain, _hail, _wind]:
			if p.playing:
				p.stop()
	if _music.playing and _tension.playing:
		var a := _music.get_playback_position()
		var b := _tension.get_playback_position()
		var length := _music.stream.get_length()
		var d := absf(a - b)
		if length > 0.0 and minf(d, length - d) > 0.06 and absf(length - _tension.stream.get_length()) < 0.05:
			_tension.seek(a)
	for th: Dictionary in _thunder:
		th.t = float(th.t) - dt
		if float(th.t) <= 0.0:
			sfx("thunder_near" if th.near else "thunder", randf_range(0.85, 1.1), float(th.vol))
	_thunder = _thunder.filter(func(th: Dictionary) -> bool: return float(th.t) > 0.0)
	_cut += ((900.0 if muffled else 20000.0) - _cut) * (1.0 - exp(-dt * 5.0))
	_lowpass.cutoff_hz = _cut
