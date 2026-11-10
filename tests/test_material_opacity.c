/* A material's opacity can be set whatever its tint: the tint is composed
 * over the canvas first, so a nearly clear light tint does not turn into
 * an opaque white card on a dark theme.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <awra/awra.h>

#include <math.h>
#include <stdio.h>

static int failures;
#define CHECK(cond, ...)                                                   \
  do {                                                                     \
    if (!(cond)) {                                                         \
      failures++;                                                          \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);                 \
      fprintf(stderr, __VA_ARGS__);                                        \
      fputc('\n', stderr);                                                 \
    }                                                                      \
  } while (0)

int main(void)
{
  awra_init("org.awra.TestMaterialOpacity");
  awra_style_set_appearance(AWRA_APPEARANCE_DARK);
  AwraRGBA canvas, layer0, layer;
  awra_style_color("canvas", &canvas);
  awra_style_color("layer", &layer0);
  const double def = awra_style_material_opacity(AWRA_MATERIAL_LAYER);
  CHECK(fabs(def - layer0.a) < 1e-4, "default opacity %f vs tint alpha %f", def, layer0.a);

  awra_style_set_material_opacity(AWRA_MATERIAL_LAYER, 1.0);
  awra_style_color("layer", &layer);
  CHECK(fabs(layer.a - 1.0) < 1e-4, "opaque layer alpha %f", layer.a);
  CHECK(fabs(awra_style_material_opacity(AWRA_MATERIAL_LAYER) - 1.0) < 1e-4, "reported opacity");
  /* composed over the dark canvas: stays dark */
  const double lum = 0.2126 * layer.r + 0.7152 * layer.g + 0.0722 * layer.b;
  const double clum = 0.2126 * canvas.r + 0.7152 * canvas.g + 0.0722 * canvas.b;
  CHECK(lum < 0.5 && fabs(lum - clum) < 0.25, "opaque layer luminance %f (canvas %f)", lum, clum);

  awra_style_set_material_opacity(AWRA_MATERIAL_LAYER, 0.4);
  awra_style_color("layer", &layer);
  CHECK(fabs(layer.a - 0.4) < 1e-4, "40%% layer alpha %f", layer.a);

  awra_style_set_material_opacity(AWRA_MATERIAL_LAYER, -1.0);
  awra_style_color("layer", &layer);
  CHECK(fabs(layer.a - layer0.a) < 1e-4 && fabs(layer.r - layer0.r) < 1e-4, "back to default");

  if (failures)
    fprintf(stderr, "%d failure(s)\n", failures);
  return failures ? 1 : 0;
}
