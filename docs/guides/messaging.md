<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Messaging applications

Chat clients, mail with several accounts, comment threads and team
workspaces share a vocabulary: people with a face and a presence, places to
switch between, activity to catch up on. Awra provides these pieces as
generic components; the application keeps its own models and decides what a
place or a person is.

## People and places: `AwraAvatar`

```c
GtkWidget *avatar = awra_avatar_new (40, "Ada Lovelace");

awra_avatar_set_custom_image (AWRA_AVATAR (avatar), texture);
awra_avatar_set_presence (AWRA_AVATAR (avatar), AWRA_PRESENCE_IDLE);
```

Without a picture the avatar shows initials on a soft colour derived from the
name, so the same name always looks the same across the application and
between sessions. Use `AWRA_AVATAR_SHAPE_ROUNDED` for places, groups and
workspaces, and keep circles for people.

A picture is cropped to fill the avatar and scaled down with mipmaps, so give
it the full-resolution texture: there is no need to scale it in advance. A
new picture cross-fades over the old one.

The presence dot is cut into the corner, so it reads on any background, and
every state has its own shape — a dot, a crescent, a bar, a ring — so it reads
without colour. `awra_avatar_set_speaking()` shows a ring that emerges around
the avatar and breathes while someone speaks; it is drawn just outside the
avatar, so leave a few pixels around it.

## Places to switch between: `AwraRail`

```c
GtkWidget *rail = awra_rail_new ();
GtkWidget *home = awra_rail_item_new (_("Home"));

awra_rail_item_set_icon_name (AWRA_RAIL_ITEM (home), "go-home-symbolic");
awra_rail_set_header (AWRA_RAIL (rail), home);

for (guint i = 0; i < n_workspaces; i++) {
  GtkWidget *item = awra_rail_item_new (workspaces[i].name);

  awra_rail_item_set_child (AWRA_RAIL_ITEM (item),
                            awra_avatar_new (48, workspaces[i].name));
  awra_rail_append (AWRA_RAIL (rail), item);
}
awra_rail_set_reorderable (AWRA_RAIL (rail), TRUE);
g_signal_connect (rail, "notify::selected-item", G_CALLBACK (switch_workspace), app);
```

Activating an item selects it and deselects the others, including items in
folders and in the header; footer items, such as one that adds a workspace,
only emit `activate`. Set `unread`, `mention-count` and `muted` from your
model: the rail draws the pill on the start edge and the mention badge, and
sums them up on a collapsed folder.

`AwraRailGroup` is a folder of items. Collapsed, it previews its first four
items or shows its icon; activated, it unfolds its items on a column tinted
with its colour.

A reorderable rail moves an entry where the user drops it and emits
`item-moved` with its new position, for the application to store. Dropping an
item onto the middle of another emits `item-merged` and changes nothing: the
application decides whether to make a folder of the two, or to add the item
to a folder it was dropped on.

Up and Down move the focus between items, Enter and Space activate. The label
of an item is its accessible name and the tooltip shown beside the rail.

## Someone is writing: `AwraTypingIndicator`

Place the indicator between the conversation and the composer, set its label
— "Ada is typing…", "Ada and 2 others are typing…" — and turn it on and off
with `active`. It keeps its height while hidden, so the conversation and the
composer never move when someone starts or stops writing.

## Reactions, filters and tags: `AwraChip`

```c
GtkWidget *reaction = awra_chip_new (NULL);

awra_chip_set_emoji (AWRA_CHIP (reaction), "🎉");
awra_chip_set_count (AWRA_CHIP (reaction), 3);
awra_chip_set_active (AWRA_CHIP (reaction), user_reacted);
g_signal_connect (reaction, "toggled", G_CALLBACK (react), message);
```

A click turns a toggleable chip on or off and emits `toggled`; changing
`active` from code emits nothing, so the application can mirror a state
confirmed by a server without loops. When the server must decide, turn
`toggleable` off and listen to `activate` instead. A custom emoji goes in
`paintable`, drawn at icon size. Update `count` as reactions arrive: the
digits that change roll, up when the count grows and down when it shrinks.
`AWRA_CHIP_APPEARANCE_ACCENT` tints a chip that suggests a choice.
