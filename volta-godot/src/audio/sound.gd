extends Node
## Audio (autoload "Sound"). The music of the web version was generative; here it is the same music, rendered
## offline by the same synthesiser into seamless 8-bar loops per theme, tempo and intensity. Every loop of a theme
## plays in lockstep with the beat clock and the intensity crossfades between them. The hero's notes are samples of
## the exact pitches of each chord; effects are rendered the same way.

const THEMES := {
	"seuil": {"root": 50, "prog": [[0, 3, 7], [-4, 0, 3], [3, 7, 10], [-2, 2, 5]], "bpms": [128]},
	"jardin": {"root": 52, "prog": [[0, 3, 7], [5, 8, 12], [-4, 0, 3], [-5, -2, 2]], "bpms": [136]},
	"fonderie": {"root": 48, "prog": [[0, 3, 7], [0, 3, 7], [-4, 0, 3], [-2, 2, 5]], "bpms": [140]},
	"ocean": {"root": 54, "prog": [[0, 3, 7], [-4, 0, 3], [5, 8, 12], [7, 11, 14]], "bpms": [118, 96, 146]},
	"ville": {"root": 45, "prog": [[0, 3, 7], [3, 7, 10], [-2, 2, 5], [-4, 0, 3]], "bpms": [150]},
	"coeur": {"root": 47, "prog": [[0, 3, 7], [-4, 0, 3], [-7, -4, 0], [-5, -1, 2]], "bpms": [160]},
	"title": {"root": 50, "prog": [[0, 3, 7, 10], [-4, 0, 3, 7], [-7, -4, 0, 3], [-2, 2, 5, 9]], "bpms": [96]},
}

var vol_music := 0.7
var vol_sfx := 0.8

var _bus_music := 0
var _bus_sfx := 0
var _lowpass: AudioEffectLowPassFilter
var _spectrum: AudioEffectSpectrumAnalyzerInstance

var _fx := AudioStreamPlayer.new()
var _fx_pb: AudioStreamPlaybackPolyphonic
var _cache: Dictionary = {}
var _last: Dictionary = {}
var _note_i := 0

# Music.
var theme := ""
var bpm := 0.0
var intensity := 2
var overdrive := false
var silence := 0.0
var _players: Array[AudioStreamPlayer] = []
var _names: Array[String] = []
var _gains: PackedFloat32Array = PackedFloat32Array()
var _loop := 1.0
var _master_gain := 1.0
var _fade := 0.0
var _playing := false
var _cut := 20000.0
var _cut_target := 20000.0
var _cut_speed := 0.3
var _stop_tw: Tween

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_setup_buses()
	_fx.bus = "Sfx"
	var poly := AudioStreamPolyphonic.new()
	poly.polyphony = 48
	_fx.stream = poly
	add_child(_fx)
	_fx.play()
	_fx_pb = _fx.get_stream_playback()
	for i in 5:
		var p := AudioStreamPlayer.new()
		p.bus = "Music"
		add_child(p)
		_players.append(p)
	_gains.resize(5)

func _setup_buses() -> void:
	# Master: the web version's compressor.
	var comp := AudioEffectCompressor.new()
	comp.threshold = -10.0
	comp.ratio = 8.0
	comp.attack_us = 3000.0
	comp.release_ms = 150.0
	AudioServer.add_bus_effect(0, comp)
	AudioServer.set_bus_volume_db(0, linear_to_db(0.9))
	var sa := AudioEffectSpectrumAnalyzer.new()
	sa.fft_size = AudioEffectSpectrumAnalyzer.FFT_SIZE_256
	AudioServer.add_bus_effect(0, sa)
	_spectrum = AudioServer.get_bus_effect_instance(0, 1)
	AudioServer.add_bus(1)
	AudioServer.set_bus_name(1, "Music")
	AudioServer.set_bus_send(1, "Master")
	_lowpass = AudioEffectLowPassFilter.new()
	_lowpass.cutoff_hz = 20000.0
	AudioServer.add_bus_effect(1, _lowpass)
	AudioServer.add_bus(2)
	AudioServer.set_bus_name(2, "Sfx")
	AudioServer.set_bus_send(2, "Master")
	_bus_music = 1
	_bus_sfx = 2
	set_volumes(vol_music, vol_sfx)

func set_volumes(music: float, sfx_v: float) -> void:
	vol_music = music
	vol_sfx = sfx_v
	AudioServer.set_bus_volume_db(_bus_music, linear_to_db(maxf(0.0001, music)))
	AudioServer.set_bus_volume_db(_bus_sfx, linear_to_db(maxf(0.0001, sfx_v)))

func _stream(path: String) -> AudioStream:
	var s: AudioStream = _cache.get(path)
	if s == null:
		if not ResourceLoader.exists(path):
			return null
		s = load(path)
		_cache[path] = s
	return s

## Low-pass sweep on the music (pause, death).
func muffle(on: bool, freq: float = 600.0) -> void:
	_cut_target = freq if on else 20000.0
	_cut_speed = 0.08 if on else 0.3

## Overall loudness 0..1 of the last few milliseconds (for reactive visuals).
func level() -> float:
	if _spectrum == null:
		return 0.0
	var m := _spectrum.get_magnitude_for_frequency_range(180.0, 9000.0, AudioEffectSpectrumAnalyzerInstance.MAGNITUDE_AVERAGE)
	var db := linear_to_db(maxf(1e-6, (m.x + m.y) * 0.5))
	return clampf((db + 70.0) / 50.0, 0.0, 1.0)

# ---- Effects ---------------------------------------------------------------------------------------------------------

func sfx(name: String, pitch: float = 1.0, vol: float = 1.0) -> void:
	var now := Time.get_ticks_msec() / 1000.0
	var gap := 0.05 if name == "enemyShot" or name == "block" else 0.025
	if _last.has(name) and now - float(_last[name]) < gap:
		return
	_last[name] = now
	var s := _stream("res://assets/audio/sfx/%s.ogg" % name)
	if s == null:
		return
	var ps := pitch
	var v := vol
	match name:
		"enemyShot":
			ps = pow(2.0, randf() * 3.0 / 12.0)
			v = vol * pitch
		"block":
			ps = 1.0
			v = vol * pitch
		"charge":
			ps = pow(2.0, 7.0 * (roundf((pitch - 0.8) / 0.15) - 1.0) / 12.0)
		"ui":
			ps = pow(2.0, pitch - 1.0)
		"die", "hurt", "bigBoom", "overdrive", "roar", "heal", "fragment", "checkpoint", "uiOk", "spawn", "lunge", "mortar", "mortarLaunch", "sniper", "phase":
			ps = 1.0
	_fx_pb.play_stream(s, 0.0, linear_to_db(maxf(0.0001, v)), ps)

## The hero's actions, as notes of the current chord.
func note(kind: String, sim_beat: float) -> void:
	var th: Dictionary = THEMES.get(theme, {})
	if th.is_empty() or theme == "title":
		return
	var s := int(floorf(sim_beat * 4.0))
	var prog: Array = th.prog
	var bar := floori(s / 16.0)
	var ch: Array = prog[posmod(bar, prog.size())]
	var tones: Array[int] = []
	for o in [0, 12, 24]:
		for n in ch:
			tones.append(int(n) + int(th.root) + o)
	var key := ""
	match kind:
		"shot":
			key = "shot_%d" % tones[(_note_i % 3) + 3]
			_note_i += 1
		"accent":
			key = "accent_%d" % (tones[3 + (_note_i % 4)] + 12)
			_note_i += 1
		"perfect":
			key = "perfect_%d_%d" % [tones[6] + 12, tones[4] + 12]
		"great":
			key = "great_%d" % (tones[5] + 12)
		"kill":
			key = "kill_%d" % (tones[2 + (_note_i % 5)] + 12)
			_note_i += 1
		"bigKill":
			key = "bigKill_%d_%d" % [tones[0] + 12, tones[2] + 12]
		"parry":
			key = "parry_%d_%d" % [tones[4] + 24, tones[6] + 12]
		"graze":
			key = "graze_%d" % (tones[7] + 12)
	var st := _stream("res://assets/audio/notes/%s.ogg" % key)
	if st:
		_fx_pb.play_stream(st, 0.0, 0.0, 1.0)

# ---- Music -------------------------------------------------------------------------------------------------------------

func _stem_names(th: String, b: float) -> Array[String]:
	var out: Array[String] = []
	if th == "title":
		out.append("title_96_0")
		return out
	var bpms: Array = THEMES[th].bpms
	var best: int = bpms[0]
	for x: int in bpms:
		if absf(x - b) < absf(best - b):
			best = x
	for i in range(1, 5):
		out.append("%s_%d_%d" % [th, best, i])
	out.append("%s_%d_od" % [th, best])
	return out

func start(th: String, b: float) -> void:
	if not THEMES.has(th):
		th = "seuil"
	theme = th
	bpm = b
	_load_stems()
	_fade = 1.0
	_master_gain = 1.0

func _load_stems() -> void:
	if _stop_tw and _stop_tw.is_valid():
		_stop_tw.kill()
	_names = _stem_names(theme, bpm)
	for i in 5:
		var p := _players[i]
		p.stop()
		if i < _names.size():
			var s := _stream("res://assets/audio/music/%s.ogg" % _names[i]) as AudioStreamOggVorbis
			if s:
				s.loop = true
				p.stream = s
				_loop = s.get_length()
		else:
			p.stream = null
		_gains[i] = 0.0
		p.volume_db = -80.0
	_playing = false

func stop(fade: float = 0.6) -> void:
	theme = ""
	if _stop_tw and _stop_tw.is_valid():
		_stop_tw.kill()
	var tw := create_tween()
	_stop_tw = tw
	for p in _players:
		if p.playing:
			tw.parallel().tween_property(p, "volume_db", -60.0, fade)
	tw.tween_callback(func() -> void:
		if theme == "":
			for p in _players:
				p.stop())
	_playing = false

## Follows the sim: keeps every loop at the position of the beat clock, crossfades intensities. Call every frame.
func update(sim_beat: float, sim_bpm: float, dt: float) -> void:
	_cut += (_cut_target - _cut) * (1.0 - exp(-dt / maxf(0.01, _cut_speed / 3.0)))
	_lowpass.cutoff_hz = _cut
	if theme == "":
		return
	if absf(sim_bpm - bpm) > 0.5:
		var old := _names.duplicate()
		bpm = sim_bpm
		if _stem_names(theme, bpm) != old:
			_load_stems()
	var spb := 60.0 / bpm
	var want := fposmod(sim_beat * spb + AudioServer.get_output_latency(), _loop)
	if not _playing:
		for p in _players:
			if p.stream:
				p.play(want)
		_playing = true
	else:
		var p0 := _players[0]
		var pos := p0.get_playback_position() + AudioServer.get_time_since_last_mix()
		var d := absf(pos - want)
		d = minf(d, _loop - d)
		if d > 0.05:
			for p in _players:
				if p.stream:
					p.seek(want)
	# Which loop: intensity 1..4, or the overdrive mix.
	var active := 0
	if theme == "title":
		active = 0
	elif overdrive:
		active = 4
	else:
		active = clampi(intensity, 1, 4) - 1
	var k := 1.0 - exp(-dt * 10.0)
	var quiet := 1.0 - silence
	for i in _players.size():
		var target := (1.0 if i == active else 0.0)
		_gains[i] += (target - _gains[i]) * k
		_players[i].volume_db = linear_to_db(maxf(0.0001, _gains[i] * quiet))
