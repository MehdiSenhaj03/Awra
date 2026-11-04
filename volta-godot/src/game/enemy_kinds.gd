class_name EnemyKinds
extends RefCounted
## Factory of the regular foes, by kind name.

static func make(k: String, x: float, y: float) -> Enemy:
	match k:
		"mote": return Mote.new(x, y)
		"spitter": return Spitter.new(x, y)
		"spinner": return Spinner.new(x, y)
		"lancer": return Lancer.new(x, y)
		"splitter": return Splitter.new(x, y)
		"bulwark": return Bulwark.new(x, y)
		"mortar": return Mortar.new(x, y)
		"sniper": return Sniper.new(x, y)
		"mirror": return Mirror.new(x, y)
		"phantom": return Phantom.new(x, y)
		"choir": return Choir.new(x, y)
		"weaver": return Weaver.new(x, y)
	push_error("unknown foe " + k)
	return Mote.new(x, y)
