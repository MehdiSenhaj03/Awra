extends Node
## Audio (autoload "Sound"). Four pieces crossfaded by mode (the title, the march, the fight, the Source), the
## ambience of the journey (wind always, an ash storm's howl, the heart forge's roar near it, fires on the deck),
## and the effects played on a polyphonic player. Menus muffle the music under the pause.

var vol_music := 0.7
var vol_sfx := 0.8
var muffled := false
var _fx := AudioStreamPlayer.new()
var _fx_pb: AudioStreamPlaybackPolyphonic
var _cache: Dictionary = {}
var _last: Dictionary = {}
var _music: Dictionary = {}          # name -> AudioStreamPlayer
var _gain: Dictionary = {}
var _mode := ""
var _amb: Dictionary = {}            # name -> AudioStreamPlayer
var _amb_t: Dictionary = {"wind": 0.0, "storm": 0.0, "forge": 0.0, "fire": 0.0}
var _amb_g: Dictionary = {"wind": 0.0, "storm": 0.0, "forge": 0.0, "fire": 0.0}
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
	poly.polyphony = 40
	_fx.stream = poly
	add_child(_fx)
	_fx.play()
	_fx_pb = _fx.get_stream_playback()
	for n in ["title", "march", "fight", "source"]:
		var p := AudioStreamPlayer.new()
		p.bus = "Music"
		p.volume_db = -80.0
		p.stream = _loop("music/" + n)
		add_child(p)
		_music[n] = p
		_gain[n] = 0.0
	for n in ["wind", "storm", "forge", "fire"]:
		var p := AudioStreamPlayer.new()
		p.bus = "Sfx"
		p.volume_db = -80.0
		p.stream = _loop("amb/" + n)
		add_child(p)
		_amb[n] = p

func _loop(n: String) -> AudioStream:
	var s := _stream("res://assets/audio/%s.ogg" % n) as AudioStreamOggVorbis
	if s:
		s.loop = true
	return s

func set_volumes(music_v: float, sfx_v: float) -> void:
	vol_music = music_v
	vol_sfx = sfx_v
	AudioServer.set_bus_volume_db(1, linear_to_db(maxf(0.0001, music_v)))
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
	if s and _fx_pb:
		_fx_pb.play_stream(s, 0.0, linear_to_db(maxf(0.0001, vol)), pitch)

## "title", "march", "fight", "source" or "" (silence).
func music(mode: String) -> void:
	if mode == _mode:
		return
	_mode = mode
	var p: AudioStreamPlayer = _music.get(mode)
	if p and p.stream and not p.playing:
		p.play()

## The journey's ambience from the simulation (called every displayed frame in a leg).
func ambience(m: March, world: World) -> void:
	_amb_t.wind = 0.35 + 0.25 * clampf(m.speed() / 27.0, 0.0, 1.0)
	_amb_t.storm = 0.9 * (world._storm_k if world else 0.0)
	_amb_t.forge = 0.15 + 0.35 * m.titan.heat / 1000.0
	_amb_t.fire = clampf(m.fires.size() * 0.4, 0.0, 0.8)

func quiet() -> void:
	for k: String in _amb_t:
		_amb_t[k] = 0.0

func _process(dt: float) -> void:
	var k := 1.0 - exp(-dt * 1.5)
	for n: String in _music:
		var target := 1.0 if n == _mode else 0.0
		_gain[n] = float(_gain[n]) + (target - float(_gain[n])) * k
		var p: AudioStreamPlayer = _music[n]
		p.volume_db = linear_to_db(maxf(0.0001, _gain[n]))
		if _gain[n] < 0.002 and p.playing and n != _mode:
			p.stop()
	var ka := 1.0 - exp(-dt * 2.5)
	for n: String in _amb:
		_amb_g[n] = float(_amb_g[n]) + (float(_amb_t[n]) - float(_amb_g[n])) * ka
		var p: AudioStreamPlayer = _amb[n]
		p.volume_db = linear_to_db(maxf(0.0001, _amb_g[n]))
		if _amb_g[n] > 0.003 and not p.playing and p.stream:
			p.play()
		elif _amb_g[n] <= 0.003 and p.playing:
			p.stop()
	_cut += ((900.0 if muffled else 20000.0) - _cut) * (1.0 - exp(-dt * 5.0))
	_lowpass.cutoff_hz = _cut
