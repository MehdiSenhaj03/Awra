/* Awra 2 style: the token set (Awra 1's AwraTokenSet, same values), the
 * style manager's preferences (appearance, accent, contrast, motion,
 * transparency, material calibration) and the desktop's UI font.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra-private.h"

#include <ctype.h>
#include <stdio.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

static AwraTokens g_tokens;
static unsigned g_gen = 1;

static struct {
  AwraAppearance appearance;
  AwraAccentSource accent_source;
  AwraRGBA custom_accent;
  bool high_contrast, reduced_motion, reduced_transparency, native_blur;
  bool tint_set[AWRA_MATERIAL_COUNT];
  AwraRGBA tint[AWRA_MATERIAL_COUNT];
  double grain_scale;
  /* desktop */
  bool system_dark;
  bool files_dark;  /* what the desktop's files say (settings.ini, kdeglobals, GTK_THEME) */
  int portal_scheme; /* the portal's color-scheme: -1 unknown, 0 none, 1 dark, 2 light */
  bool system_accent_ok;
  AwraRGBA system_accent;
  char font_family[96];
  double font_points;
  bool font_set;
} S = {AWRA_APPEARANCE_SYSTEM, AWRA_ACCENT_DEFAULT, {0.49f, 0.31f, 0.95f, 1.0f}, false, false,
       false, true};

typedef struct Listener {
  unsigned long id;
  void (*fn)(void *data);
  void *data;
  struct Listener *next;
} Listener;
static Listener *g_listeners;
static unsigned long g_next_listener = 1;

AwraRGBA awra_rgba(float r, float g, float b, float a)
{
  AwraRGBA c = {r, g, b, a};
  return c;
}

AwraRGBA awra_with_alpha(AwraRGBA c, float a)
{
  c.a = a;
  return c;
}

/* CSS mix(a, b, t): t of b over a, premultiplied like GTK's color math */
AwraRGBA awra_mix(AwraRGBA a, AwraRGBA b, float t)
{
  AwraRGBA r;
  r.a = a.a + (b.a - a.a) * t;
  if (r.a <= 0.0f) {
    return awra_rgba(0, 0, 0, 0);
  }
  r.r = (a.r * a.a + (b.r * b.a - a.r * a.a) * t) / r.a;
  r.g = (a.g * a.a + (b.g * b.a - a.g * a.a) * t) / r.a;
  r.b = (a.b * a.a + (b.b * b.a - a.b * a.a) * t) / r.a;
  return r;
}

AwraRGBA awra_shade(AwraRGBA c, float f)
{
  c.r = fminf(1.0f, c.r * f);
  c.g = fminf(1.0f, c.g * f);
  c.b = fminf(1.0f, c.b * f);
  return c;
}

static int hexv(int c)
{
  return isdigit(c) ? c - '0' : (tolower(c) - 'a' + 10);
}

bool awra_parse_color(const char *s, AwraRGBA *r)
{
  if (!s) {
    return false;
  }
  while (*s == ' ') {
    s++;
  }
  if (s[0] == '#') {
    const size_t n = strlen(s + 1);
    unsigned v[8] = {0};
    for (size_t i = 0; i < n && i < 8; i++) {
      if (!isxdigit((unsigned char)s[1 + i])) {
        return false;
      }
      v[i] = (unsigned)hexv(s[1 + i]);
    }
    if (n == 3 || n == 4) {
      r->r = v[0] * 17 / 255.0f;
      r->g = v[1] * 17 / 255.0f;
      r->b = v[2] * 17 / 255.0f;
      r->a = n == 4 ? v[3] * 17 / 255.0f : 1.0f;
      return true;
    }
    if (n == 6 || n == 8) {
      r->r = (v[0] * 16 + v[1]) / 255.0f;
      r->g = (v[2] * 16 + v[3]) / 255.0f;
      r->b = (v[4] * 16 + v[5]) / 255.0f;
      r->a = n == 8 ? (v[6] * 16 + v[7]) / 255.0f : 1.0f;
      return true;
    }
    return false;
  }
  float c[4] = {0, 0, 0, 1};
  if (sscanf(s, "rgba(%f,%f,%f,%f)", &c[0], &c[1], &c[2], &c[3]) >= 3 ||
      sscanf(s, "rgb(%f,%f,%f)", &c[0], &c[1], &c[2]) == 3 ||
      sscanf(s, "%f,%f,%f", &c[0], &c[1], &c[2]) == 3)
  {
    r->r = c[0] / 255.0f;
    r->g = c[1] / 255.0f;
    r->b = c[2] / 255.0f;
    r->a = c[3] > 1.0f ? c[3] / 255.0f : c[3];
    return true;
  }
  static const struct {
    const char *name;
    unsigned rgb;
  } named[] = {{"white", 0xffffff}, {"black", 0x000000}, {"red", 0xff0000},
               {"green", 0x008000}, {"blue", 0x0000ff}, {"gray", 0x808080},
               {"grey", 0x808080}, {"orange", 0xffa500}, {"yellow", 0xffff00},
               {"purple", 0x800080}, {"transparent", 0}};
  for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); i++) {
    if (!strcmp(s, named[i].name)) {
      r->r = ((named[i].rgb >> 16) & 255) / 255.0f;
      r->g = ((named[i].rgb >> 8) & 255) / 255.0f;
      r->b = (named[i].rgb & 255) / 255.0f;
      r->a = strcmp(s, "transparent") ? 1.0f : 0.0f;
      return true;
    }
  }
  return false;
}

/* ----------------------------------------------------------------------- */
/* The desktop's preferences */

#ifndef _WIN32
static char *read_file(const char *path)
{
  FILE *f = fopen(path, "rb");
  if (!f) {
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = n >= 0 && n < (1 << 20) ? (char *)malloc((size_t)n + 1) : NULL;
  if (buf) {
    const size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = 0;
  }
  fclose(f);
  return buf;
}

/* "key=value" in an ini-like file (the [section] is ignored). */
static bool ini_get(const char *text, const char *key, char *out, size_t size)
{
  const size_t kl = strlen(key);
  for (const char *p = text; p && *p;) {
    while (*p == ' ' || *p == '\t') {
      p++;
    }
    if (!strncmp(p, key, kl)) {
      const char *q = p + kl;
      while (*q == ' ' || *q == '\t') {
        q++;
      }
      if (*q == '=') {
        q++;
        while (*q == ' ' || *q == '\t' || *q == '"') {
          q++;
        }
        size_t n = 0;
        while (q[n] && q[n] != '\n' && q[n] != '\r') {
          n++;
        }
        while (n > 0 && (q[n - 1] == ' ' || q[n - 1] == '"')) {
          n--;
        }
        if (n >= size) {
          n = size - 1;
        }
        memcpy(out, q, n);
        out[n] = 0;
        return true;
      }
    }
    p = strchr(p, '\n');
    if (p) {
      p++;
    }
  }
  return false;
}
#endif

#ifndef _WIN32
static void config_path(char *out, size_t size, const char *rel)
{
  const char *xdg = getenv("XDG_CONFIG_HOME");
  const char *home = getenv("HOME");
  if (xdg && xdg[0]) {
    snprintf(out, size, "%s/%s", xdg, rel);
  }
  else {
    snprintf(out, size, "%s/.config/%s", home ? home : "", rel);
  }
}

/* "Noto Sans,  10" / "Noto Sans 10" / "Noto Sans,10,-1,5,..." (Qt) */
static void parse_font(const char *v, bool qt)
{
  char fam[96];
  double pt = 0;
  if (qt) {
    const char *comma = strchr(v, ',');
    if (!comma) {
      return;
    }
    snprintf(fam, sizeof(fam), "%.*s", (int)(comma - v), v);
    pt = atof(comma + 1);
  }
  else {
    snprintf(fam, sizeof(fam), "%s", v);
    char *end = fam + strlen(fam);
    while (end > fam && (end[-1] == ' ' || end[-1] == ',')) {
      *--end = 0;
    }
    char *sp = strrchr(fam, ' ');
    if (sp && atof(sp + 1) > 0) {
      pt = atof(sp + 1);
      *sp = 0;
    }
    end = fam + strlen(fam);
    while (end > fam && (end[-1] == ' ' || end[-1] == ',')) {
      *--end = 0;
    }
    /* drop style words GTK accepts after the family */
    static const char *styles[] = {" Regular", " Medium", " Book"};
    for (int i = 0; i < 3; i++) {
      const size_t l = strlen(styles[i]), fl = strlen(fam);
      if (fl > l && !strcmp(fam + fl - l, styles[i])) {
        fam[fl - l] = 0;
      }
    }
  }
  if (fam[0] && pt > 0) {
    snprintf(S.font_family, sizeof(S.font_family), "%s", fam);
    S.font_points = pt;
  }
}
#endif

static void read_desktop(void)
{
  S.system_dark = false;
  S.system_accent_ok = false;
  snprintf(S.font_family, sizeof(S.font_family), "%s", "");
  S.font_points = 10.0;
#ifdef _WIN32
  HKEY key;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0,
                    KEY_READ, &key) == ERROR_SUCCESS)
  {
    DWORD v = 1, size = sizeof(v);
    if (RegQueryValueExW(key, L"AppsUseLightTheme", NULL, NULL, (LPBYTE)&v, &size) ==
        ERROR_SUCCESS)
    {
      S.system_dark = v == 0;
    }
    RegCloseKey(key);
  }
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", 0, KEY_READ,
                    &key) == ERROR_SUCCESS)
  {
    DWORD v = 0, size = sizeof(v);
    if (RegQueryValueExW(key, L"AccentColor", NULL, NULL, (LPBYTE)&v, &size) == ERROR_SUCCESS) {
      /* 0xAABBGGRR */
      S.system_accent = awra_rgba((v & 255) / 255.0f, ((v >> 8) & 255) / 255.0f,
                                  ((v >> 16) & 255) / 255.0f, 1.0f);
      S.system_accent_ok = true;
    }
    RegCloseKey(key);
  }
  NONCLIENTMETRICSW m;
  memset(&m, 0, sizeof(m));
  m.cbSize = sizeof(m);
  if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(m), &m, 0)) {
    char fam[96];
    WideCharToMultiByte(CP_UTF8, 0, m.lfMessageFont.lfFaceName, -1, fam, sizeof(fam), NULL,
                        NULL);
    snprintf(S.font_family, sizeof(S.font_family), "%s", fam);
    HDC dc = GetDC(NULL);
    const int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc) {
      ReleaseDC(NULL, dc);
    }
    const LONG h = m.lfMessageFont.lfHeight;
    S.font_points = (h < 0 ? -h : h) * 72.0 / (dpi > 0 ? dpi : 96);
  }
  if (!S.font_family[0]) {
    snprintf(S.font_family, sizeof(S.font_family), "%s", "Segoe UI");
    S.font_points = 9.0;
  }
#else
  char path[512], v[256];
  /* GTK's own settings (KDE and GNOME write them for GTK apps) */
  for (int pass = 0; pass < 2; pass++) {
    config_path(path, sizeof(path), pass == 0 ? "gtk-4.0/settings.ini" : "gtk-3.0/settings.ini");
    char *ini = read_file(path);
    if (!ini) {
      continue;
    }
    if (ini_get(ini, "gtk-interface-color-scheme", v, sizeof(v))) { /* GTK 4.20+ */
      if (strstr(v, "dark") || !strcmp(v, "2")) {
        S.system_dark = true;
      }
      else if (strstr(v, "light") || !strcmp(v, "3")) {
        S.system_dark = false;
      }
    }
    if (ini_get(ini, "gtk-application-prefer-dark-theme", v, sizeof(v))) {
      S.system_dark = !strcmp(v, "1") || !strcmp(v, "true");
    }
    if (ini_get(ini, "gtk-theme-name", v, sizeof(v))) {
      for (char *c = v; *c; c++) {
        *c = (char)tolower((unsigned char)*c);
      }
      if (strstr(v, "dark")) {
        S.system_dark = true;
      }
    }
    if (!S.font_family[0] && ini_get(ini, "gtk-font-name", v, sizeof(v))) {
      parse_font(v, false);
    }
    free(ini);
    break;
  }
  /* KDE: the color scheme, accent and fonts */
  config_path(path, sizeof(path), "kdeglobals");
  char *kde = read_file(path);
  if (kde) {
    if (ini_get(kde, "ColorScheme", v, sizeof(v))) {
      for (char *c = v; *c; c++) {
        *c = (char)tolower((unsigned char)*c);
      }
      if (strstr(v, "dark")) {
        S.system_dark = true;
      }
    }
    if (ini_get(kde, "AccentColor", v, sizeof(v))) {
      AwraRGBA c;
      if (awra_parse_color(v, &c)) {
        S.system_accent = c;
        S.system_accent_ok = true;
      }
    }
    if (!S.font_family[0] && ini_get(kde, "font", v, sizeof(v))) {
      parse_font(v, true);
    }
    free(kde);
  }
  const char *env = getenv("GTK_THEME");
  if (env && strstr(env, ":dark")) {
    S.system_dark = true;
  }
  if (!S.font_family[0]) {
    snprintf(S.font_family, sizeof(S.font_family), "%s", "Sans"); /* GTK's default gtk-font-name */
  }
#endif
  const char *force = getenv("AWRA_APPEARANCE");
  if (force && !strcmp(force, "dark")) {
    S.system_dark = true;
  }
  else if (force && !strcmp(force, "light")) {
    S.system_dark = false;
  }
  const char *rm = getenv("AWRA_REDUCED_MOTION");
  if (rm && rm[0] == '1') {
    S.reduced_motion = true;
  }
  const char *font = getenv("AWRA_FONT"); /* "Family 10" */
#ifndef _WIN32
  if (font && font[0]) {
    parse_font(font, false);
  }
#else
  (void)font;
#endif
}

/* ----------------------------------------------------------------------- */
/* Tokens (awra-token-set.c, Awra 1.1) */

static double lin(double c)
{
  return c <= 0.03928 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

static double luminance(const AwraRGBA *c)
{
  return 0.2126 * lin(c->r) + 0.7152 * lin(c->g) + 0.0722 * lin(c->b);
}

static void ensure_readable(AwraRGBA *c, bool dark)
{
  const double canvas = dark ? 0.0168 : 0.903, target = dark ? 1.0 : 0.0;
  for (int i = 0; i < 24; i++) {
    const double l = luminance(c);
    const double ratio = dark ? (l + 0.05) / (canvas + 0.05) : (canvas + 0.05) / (l + 0.05);
    if (ratio >= 4.5) {
      return;
    }
    c->r += (float)((target - c->r) * 0.12);
    c->g += (float)((target - c->g) * 0.12);
    c->b += (float)((target - c->b) * 0.12);
  }
}

static void build_tokens(void)
{
  AwraTokens *t = &g_tokens;
  memset(t, 0, sizeof(*t));
  const bool dark = S.appearance == AWRA_APPEARANCE_DARK ||
                    (S.appearance == AWRA_APPEARANCE_SYSTEM && S.system_dark);
  const bool hc = S.high_contrast;
  t->dark = dark;
  t->high_contrast = hc;
  const AwraRGBA default_accent = {0.49f, 0.31f, 0.95f, 1.0f};
  t->accent = S.accent_source == AWRA_ACCENT_CUSTOM ? S.custom_accent :
              (S.accent_source == AWRA_ACCENT_SYSTEM && S.system_accent_ok) ? S.system_accent :
                                                                             default_accent;
  t->accent.a = 1.0f;
  const float sp[6] = {4, 8, 12, 16, 24, 32};
  memcpy(t->spacing, sp, sizeof(sp));
  const float rd[5] = {7, 11, 17, 24, 999};
  memcpy(t->radii, rd, sizeof(rd));
  t->border_width = hc ? 2.0f : 1.0f;
  t->grain_scale = (float)(S.grain_scale > 0 ? S.grain_scale : 0.20);
  AwraRGBA canvas, content, chrome, layer, floating, opaque;
  float surface_opacity;
  if (dark) {
    t->background = awra_rgba(35 / 255.0f, 35 / 255.0f, 35 / 255.0f, 1);
    t->foreground = awra_rgba(0.945f, 0.945f, 0.958f, 1);
    t->muted = awra_rgba(0.68f, 0.68f, 0.71f, 1);
    canvas = awra_rgba(35 / 255.0f, 35 / 255.0f, 35 / 255.0f, 0.60f);
    content = awra_rgba(0.14f, 0.14f, 0.15f, 0.0f);
    chrome = awra_rgba(0.035f, 0.040f, 0.050f, 0.18f);
    layer = awra_rgba(0.92f, 0.92f, 0.96f, 0.045f);
    floating = awra_rgba(37 / 255.0f, 37 / 255.0f, 37 / 255.0f, 0.60f);
    opaque = awra_rgba(35 / 255.0f, 35 / 255.0f, 35 / 255.0f, 1);
    t->outline = awra_rgba(0.96f, 0.96f, 0.99f, hc ? 0.72f : 0.10f);
    t->separator = awra_rgba(0.96f, 0.96f, 0.99f, hc ? 0.62f : 0.055f);
    t->control_well = awra_rgba(0, 0, 0, 0.28f);
    t->control_cap = awra_rgba(1, 1, 1, 0.060f);
    t->control_cap_hover = awra_rgba(1, 1, 1, 0.10f);
    t->control_thumb = awra_rgba(0.92f, 0.92f, 0.94f, 1);
    t->control_track = awra_rgba(1, 1, 1, 0.115f);
    t->rim = awra_rgba(1, 1, 1, hc ? 0.70f : 0.14f);
    surface_opacity = 0.58f;
  }
  else {
    t->background = awra_rgba(0.95f, 0.945f, 0.96f, 1);
    t->foreground = awra_rgba(0.08f, 0.07f, 0.10f, 1);
    t->muted = awra_rgba(0.31f, 0.29f, 0.35f, 1);
    canvas = awra_rgba(0.95f, 0.945f, 0.96f, 0.89f);
    content = awra_rgba(0.95f, 0.945f, 0.96f, 0.0f);
    chrome = awra_rgba(0.88f, 0.87f, 0.90f, 0.22f);
    layer = awra_rgba(1.0f, 0.995f, 1.0f, 0.40f);
    floating = awra_rgba(0.98f, 0.975f, 0.985f, 0.94f);
    opaque = awra_rgba(0.95f, 0.945f, 0.96f, 1);
    t->outline = awra_rgba(0.13f, 0.10f, 0.18f, hc ? 0.82f : 0.18f);
    t->separator = awra_rgba(0.13f, 0.10f, 0.18f, hc ? 0.72f : 0.12f);
    t->control_well = awra_rgba(0.08f, 0.065f, 0.10f, 0.105f);
    t->control_cap = awra_rgba(1, 1, 1, 0.72f);
    t->control_cap_hover = awra_rgba(1, 1, 1, 0.92f);
    t->control_thumb = awra_rgba(0.985f, 0.98f, 0.99f, 1);
    t->control_track = awra_rgba(0.10f, 0.08f, 0.14f, 0.20f);
    t->rim = awra_rgba(1, 1, 1, hc ? 0.95f : 0.68f);
    surface_opacity = 0.90f;
  }
  t->border = t->outline;
  t->control_pressed = dark ? awra_rgba(1, 1, 1, 0.135f) : awra_rgba(0.08f, 0.06f, 0.10f, 0.14f);
  t->selection = awra_with_alpha(t->accent, dark ? 0.13f : 0.14f);
  t->accent_soft = t->selection;
  t->focus_ring = awra_with_alpha(t->accent, hc ? 1.0f : 0.88f);
  t->disabled = awra_with_alpha(t->foreground, hc ? 0.68f : 0.56f);
  t->accent_content = t->accent;
  if (dark) {
    t->accent_content.r += (1.0f - t->accent_content.r) * 0.20f;
    t->accent_content.g += (1.0f - t->accent_content.g) * 0.20f;
    t->accent_content.b += (1.0f - t->accent_content.b) * 0.20f;
  }
  else {
    t->accent_content.r *= 0.72f;
    t->accent_content.g *= 0.72f;
    t->accent_content.b *= 0.72f;
  }
  ensure_readable(&t->accent_content, dark);
  t->on_accent = (0.2126f * t->accent.r + 0.7152f * t->accent.g + 0.0722f * t->accent.b) > 0.60f ?
                     awra_rgba(0.04f, 0.035f, 0.05f, 1) :
                     awra_rgba(1, 1, 1, 1);
  t->danger = awra_rgba(0.88f, 0.22f, 0.28f, 1);
  t->success = dark ? awra_rgba(0.27f, 0.76f, 0.47f, 1) : awra_rgba(0.12f, 0.58f, 0.31f, 1);
  t->warning = dark ? awra_rgba(0.96f, 0.71f, 0.26f, 1) : awra_rgba(0.84f, 0.54f, 0.06f, 1);
  t->info = dark ? awra_rgba(0.38f, 0.62f, 0.98f, 1) : awra_rgba(0.13f, 0.42f, 0.86f, 1);

  AwraRGBA *mt = t->material_tint;
  mt[AWRA_MATERIAL_SOLID] = awra_with_alpha(t->background, 1.0f);
  mt[AWRA_MATERIAL_TRANSLUCENT] = awra_with_alpha(t->background, surface_opacity);
  mt[AWRA_MATERIAL_FROSTED] = awra_with_alpha(t->background, surface_opacity);
  mt[AWRA_MATERIAL_CANVAS] = canvas;
  mt[AWRA_MATERIAL_CONTENT] = content;
  mt[AWRA_MATERIAL_CHROME] = chrome;
  mt[AWRA_MATERIAL_LAYER] = layer;
  mt[AWRA_MATERIAL_FLOATING] = floating;
  mt[AWRA_MATERIAL_OPAQUE] = opaque;
  for (int i = 0; i < AWRA_MATERIAL_COUNT; i++) {
    if (S.tint_set[i]) {
      mt[i] = S.tint[i];
    }
    t->material_radius[i] = t->radii[AWRA_RADIUS_LARGE];
    t->material_grain[i] = 0.0f;
    t->material_blur[i] = false;
  }
  t->material_radius[AWRA_MATERIAL_CANVAS] = t->radii[AWRA_RADIUS_WINDOW];
  t->material_radius[AWRA_MATERIAL_CONTENT] = 0.0f;
  t->material_grain[AWRA_MATERIAL_FROSTED] = t->grain_scale;
  t->material_grain[AWRA_MATERIAL_CANVAS] = t->grain_scale;
  t->material_grain[AWRA_MATERIAL_FLOATING] = t->grain_scale;
  t->material_blur[AWRA_MATERIAL_FROSTED] = true;
  t->material_blur[AWRA_MATERIAL_CANVAS] = true;
  t->material_blur[AWRA_MATERIAL_FLOATING] = true;
}

const AwraTokens *awra_tokens(void)
{
  return &g_tokens;
}

unsigned awra_style_generation(void)
{
  return g_gen;
}

static bool g_style_inited;
void awra_portal_start(void); /* portal.c */

static void apply_portal(void)
{
  const char *force = getenv("AWRA_APPEARANCE");
  if (force && (!strcmp(force, "dark") || !strcmp(force, "light"))) {
    return; /* the override stands */
  }
  S.system_dark = S.portal_scheme == 1 ? true : S.portal_scheme == 2 ? false : S.files_dark;
}

void awra_style_init(void)
{
  if (g_style_inited) {
    return;
  }
  g_style_inited = true;
  S.portal_scheme = -1;
  read_desktop();
  S.files_dark = S.system_dark;
  awra_portal_start(); /* may answer now, before the tokens are built */
  apply_portal();
  build_tokens();
}

static void style_changed(void);

/* portal.c: org.freedesktop.appearance color-scheme (GTK's
 * gtk-interface-color-scheme): 1 dark, 2 light, 0 no preference */
void awra_style_set_portal_scheme(int scheme);
void awra_style_set_portal_scheme(int scheme)
{
  S.portal_scheme = scheme;
  const bool was = S.system_dark;
  apply_portal();
  if (g_style_inited && S.system_dark != was && S.appearance == AWRA_APPEARANCE_SYSTEM) {
    style_changed();
  }
}

static void style_changed(void)
{
  build_tokens();
  g_gen++;
  BK_set_animations(!S.reduced_motion);
  awra_window_style_changed_all();
  for (Listener *l = g_listeners; l; l = l->next) {
    l->fn(l->data);
  }
}

void awra_style_set_appearance(AwraAppearance a)
{
  awra_style_init();
  if (S.appearance != a) {
    S.appearance = a;
    style_changed();
  }
}

AwraAppearance awra_style_appearance(void)
{
  return S.appearance;
}

bool awra_style_is_dark(void)
{
  awra_style_init();
  return g_tokens.dark;
}

void awra_style_set_accent_source(AwraAccentSource src)
{
  awra_style_init();
  if (S.accent_source != src) {
    S.accent_source = src;
    style_changed();
  }
}

void awra_style_set_accent(const AwraRGBA *c)
{
  awra_style_init();
  if (c) {
    S.custom_accent = *c;
    S.accent_source = AWRA_ACCENT_CUSTOM;
  }
  else {
    S.accent_source = AWRA_ACCENT_DEFAULT;
  }
  style_changed();
}

AwraAccentSource awra_style_accent_source(void)
{
  return S.accent_source;
}

void awra_style_set_high_contrast(bool on)
{
  awra_style_init();
  if (S.high_contrast != on) {
    S.high_contrast = on;
    style_changed();
  }
}

void awra_style_set_reduced_motion(bool on)
{
  awra_style_init();
  if (S.reduced_motion != on) {
    S.reduced_motion = on;
    style_changed();
  }
}

bool awra_style_reduced_motion(void)
{
  return S.reduced_motion;
}

void awra_style_set_reduced_transparency(bool on)
{
  awra_style_init();
  if (S.reduced_transparency != on) {
    S.reduced_transparency = on;
    style_changed();
  }
}

bool awra_reduced_transparency(void)
{
  return S.reduced_transparency;
}

void awra_style_set_native_blur(bool on)
{
  awra_style_init();
  if (S.native_blur != on) {
    S.native_blur = on;
    style_changed();
  }
}

bool awra_native_blur_enabled(void)
{
  return S.native_blur;
}

void awra_style_set_material_tint(AwraMaterial m, const AwraRGBA *tint)
{
  if (m < 0 || m >= AWRA_MATERIAL_COUNT) {
    return;
  }
  awra_style_init();
  S.tint_set[m] = tint != NULL;
  if (tint) {
    S.tint[m] = *tint;
  }
  style_changed();
}

void awra_style_set_grain_scale(double scale)
{
  awra_style_init();
  S.grain_scale = scale < 0 ? 0 : scale > 4 ? 4 : scale;
  style_changed();
}

void awra_style_reset(void)
{
  awra_style_init();
  memset(S.tint_set, 0, sizeof(S.tint_set));
  S.grain_scale = 0;
  S.high_contrast = false;
  S.reduced_transparency = false;
  S.native_blur = true;
  style_changed();
}

int awra_spacing(AwraSpacing s)
{
  awra_style_init();
  if (s < 0 || s > AWRA_SPACING_XXL) {
    return 0;
  }
  return (int)g_tokens.spacing[s];
}

int awra_radius(AwraRadius r)
{
  awra_style_init();
  if (r < 0 || r > AWRA_RADIUS_PILL) {
    return 0;
  }
  return (int)g_tokens.radii[r];
}

bool awra_style_color(const char *name, AwraRGBA *r)
{
  awra_style_init();
  const AwraTokens *t = &g_tokens;
  const struct {
    const char *name;
    const AwraRGBA *c;
  } table[] = {
      {"foreground", &t->foreground},
      {"muted", &t->muted},
      {"background", &t->background},
      {"accent", &t->accent},
      {"accent-content", &t->accent_content},
      {"accent-soft", &t->accent_soft},
      {"on-accent", &t->on_accent},
      {"danger", &t->danger},
      {"success", &t->success},
      {"warning", &t->warning},
      {"info", &t->info},
      {"outline", &t->outline},
      {"border", &t->border},
      {"separator", &t->separator},
      {"rim", &t->rim},
      {"control-cap", &t->control_cap},
      {"control-hover", &t->control_cap_hover},
      {"control-pressed", &t->control_pressed},
      {"control-well", &t->control_well},
      {"control-thumb", &t->control_thumb},
      {"control-track", &t->control_track},
      {"selection", &t->selection},
      {"focus-ring", &t->focus_ring},
      {"disabled", &t->disabled},
      {"canvas", &t->material_tint[AWRA_MATERIAL_CANVAS]},
      {"chrome", &t->material_tint[AWRA_MATERIAL_CHROME]},
      {"layer", &t->material_tint[AWRA_MATERIAL_LAYER]},
      {"floating", &t->material_tint[AWRA_MATERIAL_FLOATING]},
      {"opaque", &t->material_tint[AWRA_MATERIAL_OPAQUE]},
  };
  for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
    if (!strcmp(table[i].name, name)) {
      *r = *table[i].c;
      return true;
    }
  }
  return false;
}

unsigned long awra_style_connect_changed(void (*fn)(void *data), void *data)
{
  Listener *l = (Listener *)calloc(1, sizeof(Listener));
  if (!l) {
    return 0;
  }
  l->id = g_next_listener++;
  l->fn = fn;
  l->data = data;
  l->next = g_listeners;
  g_listeners = l;
  return l->id;
}

void awra_style_disconnect(unsigned long id)
{
  for (Listener **p = &g_listeners; *p; p = &(*p)->next) {
    if ((*p)->id == id) {
      Listener *l = *p;
      *p = l->next;
      free(l);
      return;
    }
  }
}

unsigned awra_motion_duration(AwraMotionPreset preset)
{
  static const unsigned ms[] = {0, 110, 180, 260, 260};
  if (S.reduced_motion || preset < 0 || preset > AWRA_MOTION_EMPHASIZED) {
    return 0;
  }
  return ms[preset];
}

void awra_style_set_font(const char *family, double points)
{
  awra_style_init();
  if (family && family[0]) {
    snprintf(S.font_family, sizeof(S.font_family), "%s", family);
  }
  if (points > 0) {
    S.font_points = points;
  }
  style_changed();
}

const char *awra_style_font_family(void)
{
  awra_style_init();
  return S.font_family;
}

const char *awra_font_family(void)
{
  return awra_style_font_family();
}

const char *awra_mono_family(void)
{
#ifdef _WIN32
  return "Consolas";
#else
  return "monospace";
#endif
}

double awra_style_font_size(void)
{
  awra_style_init();
  return S.font_points * 96.0 / 72.0;
}

float awra_font_px(void)
{
  return (float)awra_style_font_size();
}
