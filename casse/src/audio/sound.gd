extends Node
## Audio (autoload "Sound"). The heist music is a noir jazz loop rendered in stems; every echo on the job adds an
## instrument, and the music follows the loop's clock exactly (rewind the loop, the band rewinds too).

const LAYERS := ["clock", "bass", "drums", "rhodes", "vibes", "brass", "choir"]
const STEM := 16.0

var vol_music := 0.7
var vol_sfx := 0.8
var _fx := AudioStreamPlayer.new()
var _fx_pb: AudioStreamPlaybackPolyphonic
var _cache: Dictionary = {}
var _last: Dictionary = {}
var _players: Array[AudioStreamPlayer] = []
var _gains := PackedFloat32Array()
var _title := AudioStreamPlayer.new()
var _mode := ""
var _lowpass: AudioEffectLowPassFilter
var _cut := 20000.0
var _cut_target := 20000.0
var _title_gain := 0.0
var _heist_gain := 0.0

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	var comp := AudioEffectCompressor.new()
	comp.threshold = -8.0
	comp.ratio = 6.0
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
	for n: String in LAYERS:
		var p := AudioStreamPlayer.new()
		p.bus = "Music"
		var s := _stream("res://assets/audio/music/%s.ogg" % n) as AudioStreamOggVorbis
		if s:
			s.loop = true
			p.stream = s
		p.volume_db = -80.0
		add_child(p)
		_players.append(p)
	_gains.resize(LAYERS.size())
	_title.bus = "Music"
	var ts := _stream("res://assets/audio/music/title.ogg") as AudioStreamOggVorbis
	if ts:
		ts.loop = true
		_title.stream = ts
	_title.volume_db = -80.0
	add_child(_title)

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

## "title" or "heist".
func music(mode: String) -> void:
	if mode == _mode:
		return
	_mode = mode
	if mode == "title" and not _title.playing:
		_title.play()
	if mode == "heist":
		for p in _players:
			if p.stream and not p.playing:
				p.play()

## The band: n echoes on the job, the loop's clock, and whether it is running.
func layers(n: int, frame: int, _loop: int, playing: bool) -> void:
	var want := fposmod(frame / 60.0, STEM)
	var k := 1.0 - exp(-get_process_delta_time() * 6.0)
	for i in _players.size():
		var p := _players[i]
		if p.stream == null:
			continue
		var on := i == 0 or i <= n
		_gains[i] += ((1.0 if on else 0.0) - _gains[i]) * k
		p.volume_db = linear_to_db(maxf(0.0001, _gains[i] * _heist_gain))
		if playing and p.playing:
			var pos := p.get_playback_position() + AudioServer.get_time_since_last_mix()
			var d := absf(pos - want)
			if minf(d, STEM - d) > 0.08:
				p.seek(want)
	_cut_target = 20000.0 if playing else 1400.0

func _process(dt: float) -> void:
	var k := 1.0 - exp(-dt * 2.5)
	_title_gain += ((1.0 if _mode == "title" else 0.0) - _title_gain) * k
	_heist_gain += ((1.0 if _mode == "heist" else 0.0) - _heist_gain) * k
	_title.volume_db = linear_to_db(maxf(0.0001, _title_gain))
	if _title_gain < 0.002 and _title.playing:
		_title.stop()
	if _heist_gain < 0.002 and _mode != "heist":
		for p in _players:
			p.volume_db = -80.0
	_cut += (_cut_target - _cut) * (1.0 - exp(-dt * 5.0))
	_lowpass.cutoff_hz = _cut
	if _mode != "heist":
		_cut_target = 20000.0
