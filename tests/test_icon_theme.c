/* The icon theme looked up as GtkIconTheme does (a theme of the test's own:
 * inheritance, Fixed / Scalable / Threshold directories, scales, symbolic
 * and regular names, -rtl names, unthemed icons, image-missing), symbolic
 * icons recoloured as GTK recolours them, GThemedIcon's names, and GIO's
 * content types where shared-mime-info is installed.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include "awra/awra.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static char g_root[512];

static void put(const char *rel, const char *text)
{
  char path[1024];
  snprintf(path, sizeof(path), "%s/%s", g_root, rel);
  /* mkdir -p of the file's directory */
  for (char *p = path + strlen(g_root) + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
      mkdir(path, 0755);
      *p = '/';
    }
  }
  FILE *f = fopen(path, "wb");
  if (f) {
    fputs(text, f);
    fclose(f);
  }
}

#define SQUARE(sz) "<svg xmlns='http://www.w3.org/2000/svg' width='" #sz "' height='" #sz "'><rect width='" #sz "' height='" #sz "' fill='#3070c0'/></svg>"

/* the file a lookup chose, relative to the test's root ("" built in) */
static const char *chosen(const char *name, int size, int scale, AwraTextDirection dir, AwraIconLookupFlags flags)
{
  static char out[512];
  const char *names[1] = {name};
  AwraIconPaintable *p = awra_icon_theme_lookup_names(names, 1, size, scale, dir, flags);
  const char *f = p ? awra_icon_paintable_get_file(p) : NULL;
  snprintf(out, sizeof(out), "%s", f ? (strncmp(f, g_root, strlen(g_root)) ? f : f + strlen(g_root) + 1) : "");
  awra_icon_paintable_unref(p);
  return out;
}

#define EXPECT(name, size, scale, want)                                                      \
  do {                                                                                       \
    const char *got_ = chosen(name, size, scale, AWRA_TEXT_DIR_LTR, AWRA_ICON_LOOKUP_NONE);  \
    CHECK(!strcmp(got_, want), "%s %d@%d: %s, not %s", name, size, scale, got_, want);       \
  } while (0)

static int changed;
static void on_changed(void *data)
{
  (void)data;
  changed++;
}

static void fake_theme(void)
{
  snprintf(g_root, sizeof(g_root), "/tmp/awra-icon-theme-test-%d", (int)getpid());
  mkdir(g_root, 0755);
  /* Child inherits Parent; both before hicolor */
  put("icons/Child/index.theme",
      "[Icon Theme]\nName=Child\nInherits=Parent\n"
      "Directories=16x16/apps,24x24/apps,48x48/apps,16x16@2/apps,scalable/apps,symbolic/actions\n"
      "[16x16/apps]\nSize=16\nType=Fixed\n"
      "[24x24/apps]\nSize=24\nType=Threshold\nThreshold=4\n"
      "[48x48/apps]\nSize=48\nType=Fixed\n"
      "[16x16@2/apps]\nSize=16\nScale=2\nType=Fixed\n"
      "[scalable/apps]\nSize=64\nMinSize=32\nMaxSize=256\nType=Scalable\n"
      "[symbolic/actions]\nSize=16\nMinSize=8\nMaxSize=512\nType=Scalable\n");
  put("icons/Parent/index.theme",
      "[Icon Theme]\nName=Parent\n"
      "Directories=32x32/apps,symbolic/actions\n"
      "[32x32/apps]\nSize=32\nType=Fixed\n"
      "[symbolic/actions]\nSize=16\nMinSize=8\nMaxSize=512\nType=Scalable\n");
  put("icons/hicolor/index.theme",
      "[Icon Theme]\nName=Hicolor\nDirectories=48x48/apps\n[48x48/apps]\nSize=48\nType=Threshold\n");
  /* "app" at several sizes */
  put("icons/Child/16x16/apps/app.svg", SQUARE(16));
  put("icons/Child/24x24/apps/app.svg", SQUARE(24));
  put("icons/Child/48x48/apps/app.svg", SQUARE(48));
  put("icons/Child/16x16@2/apps/app.svg", SQUARE(32));
  put("icons/Child/scalable/apps/app.svg", SQUARE(64));
  /* a PNG beats an SVG in one directory */
  put("icons/Child/48x48/apps/both.svg", SQUARE(48));
  put("icons/Child/48x48/apps/both.png", "not really a png");
  /* only in the parent, only in hicolor */
  put("icons/Parent/32x32/apps/parent-only.svg", SQUARE(32));
  put("icons/hicolor/48x48/apps/hicolor-only.svg", SQUARE(48));
  /* symbolic: the child has the regular icon, the parent the symbolic one */
  put("icons/Child/16x16/apps/tool.svg", SQUARE(16));
  put("icons/Parent/symbolic/actions/tool-symbolic.svg",
      "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16'>"
      "<rect width='16' height='8' fill='#ff00ff'/>"
      "<rect y='8' width='16' height='8' class='error'/></svg>");
  /* right to left */
  put("icons/Child/symbolic/actions/go-next-symbolic.svg", SQUARE(16));
  put("icons/Child/symbolic/actions/go-next-symbolic-rtl.svg", SQUARE(16));
  /* GTK's own reading fails here (a transform): read as GtkSvg reads it */
  put("icons/Child/symbolic/actions/moved-symbolic.svg",
      "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16'>"
      "<rect width='8' height='16' transform='translate(8 0)' fill='#123456'/></svg>");
  /* unthemed: directly in the search path */
  put("icons/loose.svg", SQUARE(20));
}

static void cleanup(void)
{
  if (getenv("AWRA_TEST_KEEP")) {
    return; /* to compare with GTK: XDG_DATA_DIRS=<root> */
  }
  char cmd[700];
  snprintf(cmd, sizeof(cmd), "rm -rf '%s'", g_root);
  if (system(cmd) != 0) {
    fprintf(stderr, "could not remove %s\n", g_root);
  }
}

/* a pixel as premultiplied 0xAARRGGBB */
static uint32_t pixel(AwraTexture *t, int x, int y)
{
  const int w = awra_texture_width(t), h = awra_texture_height(t);
  uint8_t *rgba = (uint8_t *)malloc((size_t)w * h * 4);
  uint32_t v = 0;
  if (rgba && awra_texture_download(t, rgba, w * 4)) {
    const uint8_t *p = rgba + ((size_t)y * w + x) * 4;
    const unsigned a = p[3];
    v = (uint32_t)a << 24 | (uint32_t)((p[0] * a + 127) / 255) << 16 | (uint32_t)((p[1] * a + 127) / 255) << 8 |
        (uint32_t)((p[2] * a + 127) / 255);
  }
  free(rgba);
  return v;
}

int main(void)
{
  awra_init("org.awra.TestIconTheme");
  fake_theme();
  char icons[600];
  snprintf(icons, sizeof(icons), "%s/icons", g_root);
  const char *path[1] = {icons};
  const unsigned long id = awra_icon_theme_connect_changed(on_changed, NULL);
  awra_icon_theme_set_search_path(path, 1);
  awra_icon_theme_set_theme_name("Child");
  CHECK(changed >= 1, "no changed signal");
  char *name = awra_icon_theme_get_theme_name();
  CHECK(name && !strcmp(name, "Child"), "theme name %s", name ? name : "(null)");
  awra_free(name);

  /* sizes: exact Fixed, Threshold within 4, the scaled directory, Scalable
   * past the fixed sizes, downscaling preferred to upscaling */
  EXPECT("app", 16, 1, "icons/Child/16x16/apps/app.svg");
  EXPECT("app", 24, 1, "icons/Child/24x24/apps/app.svg");
  EXPECT("app", 22, 1, "icons/Child/24x24/apps/app.svg"); /* threshold 4 */
  EXPECT("app", 48, 1, "icons/Child/48x48/apps/app.svg");
  EXPECT("app", 16, 2, "icons/Child/16x16@2/apps/app.svg");
  EXPECT("app", 128, 1, "icons/Child/scalable/apps/app.svg");
  EXPECT("app", 40, 1, "icons/Child/scalable/apps/app.svg"); /* scalable 32..256: exact */
  EXPECT("app", 30, 1, "icons/Child/scalable/apps/app.svg");
  /* in one directory, PNG first */
  EXPECT("both", 48, 1, "icons/Child/48x48/apps/both.png");
  /* inheritance, then hicolor, then unthemed */
  EXPECT("parent-only", 16, 1, "icons/Parent/32x32/apps/parent-only.svg");
  EXPECT("hicolor-only", 16, 1, "icons/hicolor/48x48/apps/hicolor-only.svg");
  EXPECT("loose", 16, 1, "icons/loose.svg");
  /* a symbolic name: its symbolic file in any theme before a regular one */
  EXPECT("tool-symbolic", 16, 1, "icons/Parent/symbolic/actions/tool-symbolic.svg");
  /* a regular name: the regular file first; forced symbolic: the other */
  EXPECT("tool", 16, 1, "icons/Child/16x16/apps/tool.svg");
  {
    const char *got = chosen("tool", 16, 1, AWRA_TEXT_DIR_LTR, AWRA_ICON_LOOKUP_FORCE_SYMBOLIC);
    CHECK(!strcmp(got, "icons/Parent/symbolic/actions/tool-symbolic.svg"), "forced symbolic: %s", got);
  }
  /* right to left: the -rtl file */
  {
    const char *got = chosen("go-next-symbolic", 16, 1, AWRA_TEXT_DIR_RTL, 0);
    CHECK(!strcmp(got, "icons/Child/symbolic/actions/go-next-symbolic-rtl.svg"), "rtl: %s", got);
  }
  /* nothing: image-missing (GTK's, built in) */
  {
    const char *names[1] = {"no-such-icon-anywhere"};
    AwraIconPaintable *p = awra_icon_theme_lookup_names(names, 1, 16, 1, AWRA_TEXT_DIR_LTR, 0);
    CHECK(p && !awra_icon_paintable_get_file(p) && !strcmp(awra_icon_paintable_get_icon_name(p), "image-missing"),
          "missing icon: %s", p ? awra_icon_paintable_get_icon_name(p) : "(none)");
    awra_icon_paintable_unref(p);
  }
  CHECK(awra_icon_theme_has_icon("app") && awra_icon_theme_has_icon("loose") &&
            !awra_icon_theme_has_icon("no-such-icon-anywhere"),
        "has_icon");

  /* GThemedIcon: names, fallbacks, variants after them, in order */
  {
    const char *n2[2] = {"text-markdown", "x-office-document"};
    char *s = awra_themed_icon_names(n2, 2, false);
    CHECK(s && !strcmp(s, "text-markdown\nx-office-document\ntext-markdown-symbolic\nx-office-document-symbolic"),
          "themed names: %s", s ? s : "(null)");
    awra_free(s);
    const char *n1[1] = {"a-b-c-symbolic"};
    s = awra_themed_icon_names(n1, 1, true);
    CHECK(s && !strcmp(s, "a-b-c-symbolic\na-b-symbolic\na-symbolic\na-b-c\na-b\na"), "fallbacks: %s",
          s ? s : "(null)");
    awra_free(s);
  }

  /* symbolic colours: the shape without a class in the foreground whatever
   * its fill, the "error" one in GTK's error colour */
  {
    const char *names[1] = {"tool-symbolic"};
    AwraIconPaintable *p = awra_icon_theme_lookup_names(names, 1, 16, 1, AWRA_TEXT_DIR_LTR, 0);
    const AwraRGBA white = {1, 1, 1, 1};
    AwraTexture *t = awra_icon_paintable_render(p, &white);
    CHECK(t != NULL, "no render");
    if (t) {
      const uint32_t top = pixel(t, 8, 3), bottom = pixel(t, 8, 12);
      CHECK(top == 0xffffffffu, "foreground part: %08x", top);
      CHECK(bottom == 0xffcc0000u, "error part: %08x", bottom);
      awra_texture_unref(t);
    }
    awra_icon_paintable_unref(p);
    /* GtkSvg's reading: the transform applies, the fill doesn't */
    names[0] = "moved-symbolic";
    p = awra_icon_theme_lookup_names(names, 1, 16, 1, AWRA_TEXT_DIR_LTR, 0);
    t = awra_icon_paintable_render(p, &white);
    if (t) {
      CHECK(pixel(t, 3, 8) >> 24 == 0, "left half drawn: %08x", pixel(t, 3, 8));
      CHECK(pixel(t, 12, 8) == 0xffffffffu, "right half: %08x", pixel(t, 12, 8));
      awra_texture_unref(t);
    }
    awra_icon_paintable_unref(p);
  }

  /* the user's themes, as GTK 4.22 chooses there (checked against it) */
  struct stat st;
  awra_icon_theme_set_search_path(NULL, 0);
  awra_icon_theme_disconnect(id);
  {
    /* the default search path again */
    awra_icon_theme_set_theme_name(NULL);
  }
  char *sp = awra_icon_theme_get_search_path();
  CHECK(sp != NULL, "no search path");
  awra_free(sp);
  if (stat("/usr/share/icons/Papirus-Dark/icon-theme.cache", &st) == 0) {
    static const char *const sys[] = {"/usr/share/icons", "/usr/share/pixmaps"};
    awra_icon_theme_set_search_path(sys, 2);
    awra_icon_theme_set_theme_name("Papirus-Dark");
    g_root[0] = 0;
    strcpy(g_root, "/usr/share");
    EXPECT("go-previous-symbolic", 16, 1, "icons/Papirus-Dark/symbolic/actions/go-previous-symbolic.svg");
    EXPECT("folder", 48, 1, "icons/Papirus-Dark/48x48/places/folder.svg");
    EXPECT("folder", 16, 2, "icons/Papirus-Dark/16x16@2x/places/folder.svg");
    EXPECT("application-pdf", 32, 1, "icons/Papirus-Dark/32x32/mimetypes/application-pdf.svg");
    EXPECT("open-menu-symbolic", 16, 1, "icons/Papirus-Dark/symbolic/actions/open-menu-symbolic.svg");
    EXPECT("window-close-symbolic", 16, 1, "icons/Papirus-Dark/symbolic/actions/window-close-symbolic.svg");
  }
  else {
    printf("Papirus-Dark not installed: its checks skipped\n");
  }
  awra_icon_theme_set_theme_name(NULL);

  /* content types (shared-mime-info) */
  if (stat("/usr/share/mime/mime.cache", &st) == 0) {
    bool uncertain = true;
    char *t = awra_content_type_guess("Rapport.PDF", NULL, 0, &uncertain);
    CHECK(t && !strcmp(t, "application/pdf") && !uncertain, "pdf: %s", t ? t : "(null)");
    awra_free(t);
    t = awra_content_type_guess("archive.tar.gz", NULL, 0, NULL);
    CHECK(t && !strcmp(t, "application/x-compressed-tar"), "tar.gz: %s", t ? t : "(null)");
    awra_free(t);
    t = awra_content_type_guess("noext", "%PDF-1.5\n", 9, &uncertain);
    CHECK(t && !strcmp(t, "application/pdf"), "pdf data: %s", t ? t : "(null)");
    awra_free(t);
    t = awra_content_type_guess("noext", "\x01\x02\x03", 3, &uncertain);
    CHECK(t && !strcmp(t, "application/octet-stream") && uncertain, "binary: %s", t ? t : "(null)");
    awra_free(t);
    char *icons_ = awra_content_type_get_icon_names("application/pdf");
    CHECK(icons_ && !strncmp(icons_, "application-pdf\n", 16), "pdf icons: %s", icons_ ? icons_ : "(null)");
    awra_free(icons_);
    CHECK(awra_content_type_is_a("text/markdown", "text/plain"), "markdown is text");
    CHECK(awra_content_type_is_a("application/pdf", "application/octet-stream"), "everything is data");
    CHECK(!awra_content_type_is_a("inode/directory", "application/octet-stream"), "but directories");
    char *g = awra_content_type_get_generic_icon_name("video/x-matroska");
    CHECK(g && !strcmp(g, "video-x-generic"), "generic: %s", g ? g : "(null)");
    awra_free(g);
  }
  else {
    printf("shared-mime-info not installed: content types skipped\n");
  }
  snprintf(g_root, sizeof(g_root), "/tmp/awra-icon-theme-test-%d", (int)getpid());
  cleanup();
  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  printf("ok\n");
  return 0;
}
