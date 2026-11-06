<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# ADR-0003: Choose deliberately between widget, role and material

Status: accepted.

Matter, depth and silhouette belong to Material/Surface. Reusable lifecycle,
focus, keyboard or accessibility behaviour belongs to a typed Awra widget.
Presentation of an existing GTK behavioural widget may use a public Awra role,
translated internally to an `awra-role-*` class. Domain concepts remain
application compositions. This prevents both stringly typed behaviour and a
low-value wrapper around every GTK class.
