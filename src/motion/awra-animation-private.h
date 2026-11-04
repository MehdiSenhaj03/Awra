/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <awra/awra-animation.h>

G_BEGIN_DECLS

struct _AwraAnimationTarget {
  GObject parent_instance;
};

struct _AwraAnimationTargetClass {
  GObjectClass parent_class;
  void (*set_value) (AwraAnimationTarget *self,
                     double               value);
};

void awra_animation_target_set_value (AwraAnimationTarget *self,
                                      double               value);

struct _AwraAnimation {
  GObject parent_instance;
};

struct _AwraAnimationClass {
  GObjectClass parent_class;
  /* Milliseconds until the end, or AWRA_DURATION_INFINITE. */
  guint  (*estimate_duration) (AwraAnimation *self);
  /* The value @elapsed milliseconds after the start of the clock. */
  double (*calculate_value)   (AwraAnimation *self,
                               double         elapsed);
};

/* Milliseconds since the clock started, as of the current frame. */
double awra_animation_get_elapsed     (AwraAnimation *self);
/* Restarts the clock from the current frame without changing the state,
 * for a subclass whose motion changed in flight. */
void   awra_animation_restart_clock   (AwraAnimation *self);
/* Recomputes the value at the current elapsed time and hands it out. */
void   awra_animation_refresh_value   (AwraAnimation *self);

/* The nominal duration of a preset, ignoring reduced motion. */
guint  awra_motion_preset_get_nominal_duration (AwraMotionPreset preset);
/* Whether reduced motion is on for the display of @widget. */
gboolean awra_motion_is_reduced_for_widget (GtkWidget *widget);

G_END_DECLS
