<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Getting started

After installing Awra, compile an application with:

```sh
cc app.c $(pkg-config --cflags --libs awra-1)
```

A minimal application creates an ordinary `GtkApplication` and an Awra window:

```c
#include <awra/awra.h>

static void
activate (GtkApplication *app)
{
  GtkWidget *window = awra_window_new (app);
  GtkWidget *page = awra_page_new ();
  GtkWidget *content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *header = awra_page_header_new ();
  GtkWidget *section = awra_section_new ();
  GtkWidget *button = awra_button_new_with_label ("Hello");

  awra_box_set_spacing (GTK_BOX (content), AWRA_SPACING_XL);
  awra_page_header_set_title (AWRA_PAGE_HEADER (header), "Library");
  awra_page_header_set_subtitle (AWRA_PAGE_HEADER (header),
                                 "A responsive Awra page");
  awra_section_set_title (AWRA_SECTION (section), "First action");
  awra_section_set_child (AWRA_SECTION (section), button);
  gtk_box_append (GTK_BOX (content), header);
  gtk_box_append (GTK_BOX (content), section);
  awra_page_set_child (AWRA_PAGE (page), content);
  gtk_window_set_title (GTK_WINDOW (window), "Awra app");
  awra_window_set_content (AWRA_WINDOW (window), page);
  gtk_window_present (GTK_WINDOW (window));
}
```

Applications do not need a CSS file or compositor calls. Set
`AWRA_FORCE_FALLBACK=1` when testing the opaque fallback path.

`AwraLayoutPreset` is the normal starting point for composition. `PAGE`,
`SECTION`, `CONTENT`, `TOOLBAR`, `OVERLAY`, `CONTROL_GROUP` and `COMPACT`
apply coordinated inset and gap tokens to ordinary `GtkBox` or `GtkGrid`
containers. Use `AwraSpacing` helpers only when a layout needs a deliberate
exception. `AwraCard` already owns an LG content inset and `AwraSidebar` an MD
outer inset, so their children should not duplicate those margins.

For conventional screens, prefer `AwraPage`, `AwraPageHeader`, `AwraSection`
and `AwraToolbar`: they own scrolling, readable width, page insets and the
heading/action rhythm. The lower-level layout presets remain available for
atypical compositions. See [page-composition.md](page-composition.md).

# Language bindings and Blueprint

Python loads `gi.repository.Awra` version `1.0`, and Vala consumers use
`--pkg awra-1`. The optional `awra-example.blp` imports `Awra 1.0`; Meson
validates it automatically when `blueprint-compiler` is available.
