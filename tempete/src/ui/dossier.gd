class_name DossierScreen
extends Screen
## The chase dossier: the best photograph of each category as polaroids, the probes' data, the total against the
## day's goal (or the rival's), what the day earned and what the truck costs to fix.

var _menu: UIMenu

func _init(r: Dictionary, money: Dictionary, goal: int, actions: Array) -> void:
	super()
	dim(0.86)
	var dos: Dictionary = r.dossier
	var title := Kit.label(self, I18n.t("dossier.title"), 64, Kit.GOLD, "display", 700, 0.08, Color(0.5, 0.25, 0.0, 0.4), 14)
	title.position = Vector2(110, 60)
	var album: Dictionary = r.album
	# Polaroids, one per category.
	for c in 4:
		var x := 110.0 + c * 430.0
		var card := Control.new()
		card.position = Vector2(x, 180)
		card.size = Vector2(400, 330)
		card.rotation = [-0.03, 0.02, -0.015, 0.03][c]
		card.mouse_filter = Control.MOUSE_FILTER_IGNORE
		add_child(card)
		Kit.rect(card, Color(0.95, 0.94, 0.9), Vector2.ZERO, Vector2(400, 330))
		var a: Dictionary = album.get(c, {})
		if a.has("img") and a.img != null:
			var tr := TextureRect.new()
			tr.texture = ImageTexture.create_from_image(a.img)
			tr.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
			tr.position = Vector2(14, 14)
			tr.size = Vector2(372, 209)
			card.add_child(tr)
		else:
			Kit.rect(card, Color(0.15, 0.16, 0.18), Vector2(14, 14), Vector2(372, 209))
			var none := Kit.label(card, I18n.t("dossier.none"), 18, Color(0.6, 0.6, 0.62), "display", 500)
			none.position = Vector2(14, 100)
			none.size = Vector2(372, 30)
			none.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		var l := Kit.label(card, I18n.t("cat.%s" % Chase.CATS[c]).to_upper(), 22, Color(0.12, 0.12, 0.14), "display", 700, 0.1)
		l.position = Vector2(20, 240)
		var s := Kit.label(card, str(int((dos.best as Array)[c])), 40, Color(0.75, 0.3, 0.05), "display", 700)
		s.position = Vector2(20, 268)
		card.modulate.a = 0.0
		var tw := card.create_tween()
		tw.tween_interval(0.2 + c * 0.25)
		tw.tween_property(card, "modulate:a", 1.0, 0.3)
		tw.parallel().tween_property(card, "position:y", 180.0, 0.4).from(230.0).set_ease(Tween.EASE_OUT).set_trans(Tween.TRANS_BACK)
	var list := Kit.vbox(self, 0)
	list.position = Vector2(110, 560)
	list.custom_minimum_size.x = 900
	var rows: Array = [
		[I18n.t("dossier.photos"), str(int(dos.total) - int(dos.probes))],
		[I18n.t("dossier.probes"), str(dos.probes)],
		[I18n.t("dossier.total"), "%d  /  %d" % [dos.total, goal] if goal > 0 else str(dos.total)],
	]
	if r.has("rival"):
		rows.append([I18n.t("dossier.rival"), str((r.rival as Dictionary).total)])
	if not money.is_empty():
		rows.append([I18n.t("dossier.earned"), "+ $%d" % money.earned])
		rows.append([I18n.t("dossier.repair"), "− $%d" % money.repair])
	for i in rows.size():
		var rr: Array = rows[i]
		var row := Control.new()
		row.custom_minimum_size = Vector2(900, 46)
		row.mouse_filter = Control.MOUSE_FILTER_IGNORE
		var k := Kit.label(row, rr[0], 21, Kit.SOFT, "body", 600)
		k.position = Vector2(0, 10)
		var v := Kit.label(row, rr[1], 26, Kit.IVORY, "display", 700)
		v.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		v.position = Vector2(400, 6)
		v.size = Vector2(500, 40)
		Kit.rect(row, Color(1, 0.8, 0.5, 0.12), Vector2(0, 45), Vector2(900, 1))
		list.add_child(row)
	var verdict := ""
	if not bool(dos.alive):
		verdict = I18n.t("dossier.wrecked")
	elif money.has("ok"):
		verdict = I18n.t("dossier.ok") if money.ok else I18n.t("dossier.short")
	elif r.has("rival"):
		verdict = I18n.t("dossier.beat") if int(dos.total) > int((r.rival as Dictionary).total) else I18n.t("dossier.beaten")
	var vl := Kit.label(self, verdict, 30, Kit.GOLD, "display", 700, 0.06)
	vl.position = Vector2(1100, 580)
	vl.size = Vector2(700, 50)
	var menu_box := Kit.vbox(self, 0)
	menu_box.position = Vector2(1100, 660)
	_menu = UIMenu.new(menu_box)
	for a: Dictionary in actions:
		_menu.add(a.label, a.action, {"size": 22})
	Sound.sfx("score")

func update(_dt: float) -> void:
	_menu.handle()
