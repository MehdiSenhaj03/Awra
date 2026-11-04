class_name Bosses
extends RefCounted
## The Conductors, by id.

static func make(id: String, w: World) -> Enemy:
	match id:
		"ostinato": return Ostinato.new(w)
		"crescendo": return Crescendo.new(w)
		"fugue": return Fugue.make(w)
		"rubato": return Rubato.new(w)
		"staccato": return Staccato.new(w)
		"fermata": return Fermata.new(w)
	push_error("unknown boss " + id)
	return Ostinato.new(w)
