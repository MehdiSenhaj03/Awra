<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Conversations, threads and logs

Messaging apps, comment threads, activity feeds, log viewers and consoles share
one shape: a timeline that grows at the bottom, read from the newest item
upwards, with something to write at the end. Awra provides the pieces as
generic widgets; the application keeps its own vocabulary and data.

## The timeline: `AwraConversationView`

```c
GtkWidget *view = awra_conversation_view_new (G_LIST_MODEL (messages), factory);

g_signal_connect (view, "edge-reached", G_CALLBACK (load_more_cb), app);
g_signal_connect (view, "visible-range-changed", G_CALLBACK (mark_read_cb), app);
```

The model holds the items oldest first. The view is virtualized (tens of
thousands of items cost a screenful of rows) and starts at the end.

- **Following.** `AwraConversationView:follow` is on by default: items appended
  while the end is in view glide into place and rise in. Only appended items
  animate; rows recycled by scrolling never do. Scrolling up turns following
  off, reaching the end turns it on again. `awra_conversation_view_set_follow()`
  overrides it, for example for a window of history that does not reach the
  present.
- **Older history.** Insert older items at position 0 when `edge-reached`
  reports `GTK_POS_TOP`: what is on screen does not move. The signal is
  emitted again when items were inserted and the view is still near the edge,
  so loading continues until the screen is filled. `edge-threshold` sets the
  distance; by default it is the height of the view.
- **Jumping.** Away from the end, a pill at the bottom offers to return. Set
  `unread-count` to have it announce new items ("3 new messages") or
  `jump-label` to replace the text. Clicking it emits `jump-activated`, whose
  default handler calls `awra_conversation_view_scroll_to_end()`.
- **Reaching an item.** `awra_conversation_view_scroll_to_item()` places an
  item at the top, centre or bottom, gliding when it is near, and can flash it
  so the eye finds a quoted message or a search result.
- **Reading position.** `awra_conversation_view_get_visible_range()` returns the
  first and last visible positions; `visible-range-changed` is emitted at most
  once per frame.
- **Sections.** When the model implements `GtkSectionModel`, the
  `header-factory` builds a header above each section, such as a separator
  for each day.

Rows come from a `GtkSignalListItemFactory` or a `GtkBuilderListItemFactory`.
Rows carry no card: they get a quiet hover and the view's own marks. Page Up,
Page Down, Home and End scroll the view while it has the focus. Everything
follows the reduced-motion preference: glides jump, rows appear in place and
a highlight stays still for the same time.

## Writing: `AwraComposer`

```c
GtkWidget *composer = awra_composer_new ();

awra_composer_set_placeholder_text (AWRA_COMPOSER (composer), "Message #general");
awra_composer_set_start_widget (AWRA_COMPOSER (composer), add_button);
awra_composer_set_end_widget (AWRA_COMPOSER (composer), emoji_button);
g_signal_connect (composer, "submit", G_CALLBACK (send_cb), app);
```

One rounded well holds an optional banner (`awra_composer_show_banner()` makes
a line of text with a close button, which emits `banner-closed`), an optional
`attachments` widget, then the start widget, the text, the end widget and a
send button. Banner and attachments slide in and out; the send button grows in
when there is something to send (`can-send` forces it, for attachments alone).

The text grows from one line to `max-lines`, then scrolls, and the well glides
to each new height. Enter emits `submit` and Shift+Enter inserts a new line;
with `send-on-enter` off, Ctrl+Enter sends. Enter never submits while an input
method is composing or an `AwraCompletionPopover` attached to the text is
shown. The `submit` handler reads `awra_composer_get_text()` and clears it with
`awra_composer_set_text (composer, NULL)`. `busy` shows progress on the send
button. `awra_composer_get_view()` returns the `GtkTextView` for tags, inline
images (child anchors) and further key handling.

## Suggestions while typing: `AwraCompletionPopover`

```c
AwraCompletionPopover *mentions = AWRA_COMPLETION_POPOVER (awra_completion_popover_new ());

awra_completion_popover_set_model (mentions, G_LIST_MODEL (filtered_people));
awra_completion_popover_set_factory (mentions, person_factory);
awra_completion_popover_attach (mentions, GTK_WIDGET (awra_composer_get_view (composer)));
g_signal_connect (mentions, "activated", G_CALLBACK (insert_mention_cb), app);
```

The application decides when a word deserves suggestions (after `@`, `:` or
`/`), filters the model, and calls
`awra_completion_popover_present_at_cursor()`; it follows the caret when
called again. While shown, the text keeps the focus: Up and Down move the
selection and wrap around, Tab and Enter emit `activated` with the position,
Escape closes it and emits `dismissed`. Hovering selects and clicking
activates. An `AwraComposer` never sends on Enter while a completion attached
to its text view is shown. The popover works with a `GtkTextView` or any
`GtkEditable`, belongs to the widget it is attached to, and hides when the
model empties or the text loses the focus.

