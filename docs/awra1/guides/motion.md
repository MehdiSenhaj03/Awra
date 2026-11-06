<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Motion

Awra widgets animate themselves. When an application draws something of its
own — a custom indicator, a value that eases between states, a panel that
follows the pointer — it uses the same machinery: an animation object driven
by the widget's frame clock, which honours reduced motion and stops when the
widget leaves the screen. There is no tick callback to write.

## Timed animations

An `AwraTimedAnimation` goes from one value to another over a duration,
following an `AwraEasing` curve. Take the duration from the semantic presets,
so every part of an application moves at the same pace:

```c
static void
set_reveal (double   value,
            gpointer user_data)
{
  MyIndicator *self = user_data;

  self->reveal = value;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

self->reveal_animation =
  awra_timed_animation_new_for_preset (GTK_WIDGET (self), 0.0, 1.0,
                                       AWRA_MOTION_PRESET_NORMAL,
                                       awra_callback_animation_target_new (set_reveal, self, NULL));
awra_animation_play (self->reveal_animation);
```

Draw with the value in `snapshot()`, using a transform or an opacity: a
value that only changes what is drawn should never queue a resize.

A timed animation can repeat (`repeat-count`, 0 for ever), run backwards
(`reverse`) and alternate its direction on each repetition (`alternate`),
which gives a pulse. `awra_easing_ease()` evaluates the same curves for code
that computes its own progress.

## Springs and interruption

An `AwraSpringAnimation` follows a damped spring instead of a clock. It suits
anything the user can interrupt: a hover that comes and goes, an indicator
that grows and shrinks, a count that pops. Call
`awra_spring_animation_retarget()` when the destination changes: the spring
keeps its current value and velocity and bends towards the new destination,
so rapid changes never jump or restart from rest.

```c
/* On hover, on leave, on selection: one call, whatever is in flight. */
awra_spring_animation_retarget (AWRA_SPRING_ANIMATION (self->height), target_height);
```

The damping ratio sets the character of the motion: 1 arrives as fast as
possible without overshoot, 0.7–0.85 overshoots a little and settles, which
reads as lively without being playful. A stiffness between 200 and 600 keeps
springs in the range of Awra's semantic durations. Set `epsilon` to the
precision that matters for the value — about 0.1 for pixels — so the spring
ends as soon as further motion would be invisible. `clamp` ends the motion
the first time it reaches its destination, for values that must stay in
range such as an opacity.

## Lifecycle

An animation holds its widget weakly and keeps itself alive while it plays,
so an application can start one and drop its reference. It jumps to its end,
gives the target its final value and emits `done` when:

- reduced motion is on (unless `follow-reduced-motion` is turned off, for
  motion that carries meaning such as progress);
- the widget is not mapped when it starts;
- the widget is unmapped while it plays.

The target always ends in a consistent state, and a widget that keeps an
animation can release it in `dispose()` without stopping it first.

`AwraPropertyAnimationTarget` writes the value to a numeric property of any
object instead, holding the object weakly; integer properties receive the
rounded value.

From Rust, targets are closures:

```rust
let target = awra::CallbackAnimationTarget::new(glib::clone!(
    #[weak] widget, move |value| widget.set_opacity(value)
));
let animation = awra::SpringAnimation::new(&widget, 0.0, 1.0, 0.85, 400.0, target);
animation.play();
```
