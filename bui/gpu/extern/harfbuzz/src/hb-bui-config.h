/* BUI: HarfBuzz built with HB_LEAN for shaping as Pango uses it, keeping
 * what BUI's text engine needs:
 *  - font variations (variable faces such as Inter: wght, opsz),
 *  - GDEF ligature carets (caret positions inside ligatures, as Pango
 *    places them),
 * and leaving out drawing, painting and colour (FreeType draws glyphs).
 *
 * Included by hb-config.hh through HB_CONFIG_OVERRIDE_H. */
#undef HB_NO_VAR
#undef HB_NO_LAYOUT_UNUSED
#define HB_NO_DRAW
#define HB_NO_PAINT
#define HB_NO_COLOR
