# SPDX-License-Identifier: LGPL-2.1-or-later

project(
  'awra',
  'c',
  version: '1.1.0',
  license: 'LGPL-2.1-or-later',
  meson_version: '>= 1.3.0',
  default_options: [
    'c_std=c17',
    'warning_level=2',
    'werror=true',
  ],
)

gnome = import('gnome')
i18n = import('i18n')
pkgconfig = import('pkgconfig')
fs = import('fs')
vala_available = add_languages('vala', native: false, required: false)

cc = meson.get_compiler('c')
python = find_program('python3')
gtk_dep = dependency('gtk4', version: '>= 4.22.0')
gio_dep = dependency('gio-2.0', version: '>= 2.80')
wayland_dep = dependency(
  'wayland-client',
  required: get_option('wayland-effects'),
)
wayland_scanner = find_program(
  'wayland-scanner',
  required: get_option('wayland-effects'),
)

conf = configuration_data()
conf.set_quoted('AWRA_VERSION', meson.project_version())
conf.set_quoted('AWRA_LOCALEDIR', get_option('prefix') / get_option('localedir'))
conf.set('AWRA_HAVE_WAYLAND_EFFECTS', wayland_dep.found() and wayland_scanner.found())
conf.set('AWRA_HAVE_WIN32', host_machine.system() == 'windows')
conf.set('AWRA_DEVELOPER_MODE', get_option('developer'))
configure_file(output: 'awra-config.h', configuration: conf)

add_project_arguments(
  '-DG_LOG_USE_STRUCTURED=1',
  language: 'c',
)

inc_public = include_directories('include/awra-1')
inc_private = include_directories('src', 'src/platform')

# The version header is read by every consumer as is (no generated header
# to find): it must say the project's version.
if not fs.read('include/awra-1/awra/awra-version.h').contains(
    '#define AWRA_VERSION_S "' + meson.project_version() + '"')
  error('include/awra-1/awra/awra-version.h does not match version ' +
        meson.project_version())
endif

public_headers = files(
  'include/awra-1/awra/awra.h',
  'include/awra-1/awra/awra-version.h',
  'include/awra-1/awra/awra-enums.h',
  'include/awra-1/awra/awra-token-set.h',
  'include/awra-1/awra/awra-typography.h',
  'include/awra-1/awra/awra-layout.h',
  'include/awra-1/awra/awra-style-manager.h',
  'include/awra-1/awra/awra-style-role.h',
  'include/awra-1/awra/awra-material.h',
  'include/awra-1/awra/awra-material-resolution.h',
  'include/awra-1/awra/awra-diagnostics.h',
  'include/awra-1/awra/awra-context.h',
  'include/awra-1/awra/awra-surface.h',
  'include/awra-1/awra/awra-button.h',
  'include/awra-1/awra/awra-window.h',
  'include/awra-1/awra/awra-popover.h',
  'include/awra-1/awra/awra-menu-button.h',
  'include/awra-1/awra/awra-context-menu.h',
  'include/awra-1/awra/awra-card.h',
  'include/awra-1/awra/awra-page.h',
  'include/awra-1/awra/awra-page-header.h',
  'include/awra-1/awra/awra-section.h',
  'include/awra-1/awra/awra-toolbar.h',
  'include/awra-1/awra/awra-form-row.h',
  'include/awra-1/awra/awra-form.h',
  'include/awra-1/awra/awra-empty-state.h',
  'include/awra-1/awra/awra-loading-state.h',
  'include/awra-1/awra/awra-error-state.h',
  'include/awra-1/awra/awra-filter-bar.h',
  'include/awra-1/awra/awra-master-detail.h',
  'include/awra-1/awra/awra-detail-pane.h',
  'include/awra-1/awra/awra-data-column.h',
  'include/awra-1/awra/awra-data-view.h',
  'include/awra-1/awra/awra-selection-toolbar.h',
  'include/awra-1/awra/awra-metric-row.h',
  'include/awra-1/awra/awra-stat-bar.h',
  'include/awra-1/awra/awra-badge-group.h',
  'include/awra-1/awra/awra-metadata-group.h',
  'include/awra-1/awra/awra-text-area.h',
  'include/awra-1/awra/awra-password-entry.h',
  'include/awra-1/awra/awra-tag-entry.h',
  'include/awra-1/awra/awra-choice-group.h',
  'include/awra-1/awra/awra-status-banner.h',
  'include/awra-1/awra/awra-skeleton.h',
  'include/awra-1/awra/awra-header.h',
  'include/awra-1/awra/awra-sidebar.h',
  'include/awra-1/awra/awra-split-view.h',
  'include/awra-1/awra/awra-responsive-bin.h',
  'include/awra-1/awra/awra-edge-panel.h',
  'include/awra-1/awra/awra-overlay-bar.h',
  'include/awra-1/awra/awra-toggle-button.h',
  'include/awra-1/awra/awra-entry.h',
  'include/awra-1/awra/awra-search-entry.h',
  'include/awra-1/awra/awra-switch.h',
  'include/awra-1/awra/awra-slider.h',
  'include/awra-1/awra/awra-check-button.h',
  'include/awra-1/awra/awra-spin-button.h',
  'include/awra-1/awra/awra-dropdown.h',
  'include/awra-1/awra/awra-dialog.h',
  'include/awra-1/awra/awra-command-palette.h',
  'include/awra-1/awra/awra-toast-overlay.h',
  'include/awra-1/awra/awra-navigation-view.h',
  'include/awra-1/awra/awra-breadcrumb-item.h',
  'include/awra-1/awra/awra-breadcrumb-bar.h',
  'include/awra-1/awra/awra-path-bar.h',
  'include/awra-1/awra/awra-navigation-item.h',
  'include/awra-1/awra/awra-motion.h',
  'include/awra-1/awra/awra-badge.h',
  'include/awra-1/awra/awra-progress-bar.h',
  'include/awra-1/awra/awra-segmented-control.h',
  'include/awra-1/awra/awra-tab-view.h',
  'include/awra-1/awra/awra-list-view.h',
  'include/awra-1/awra/awra-grid-view.h',
  'include/awra-1/awra/awra-zoom-controller.h',
  'include/awra-1/awra/awra-action-row.h',
  'include/awra-1/awra/awra-inspector.h',
  'include/awra-1/awra/awra-key-cap.h',
  'include/awra-1/awra/awra-keyboard-view.h',
  'include/awra-1/awra/awra-shortcut-capture.h',
  'include/awra-1/awra/awra-waveform-view.h',
  'include/awra-1/awra/awra-timeline.h',
  'include/awra-1/awra/awra-transition-bin.h',
  'include/awra-1/awra/awra-animation-target.h',
  'include/awra-1/awra/awra-animation.h',
  'include/awra-1/awra/awra-timed-animation.h',
  'include/awra-1/awra/awra-spring-animation.h',
  'include/awra-1/awra/awra-avatar.h',
  'include/awra-1/awra/awra-tile-view.h',
  'include/awra-1/awra/awra-aspect-paintable.h',
  'include/awra-1/awra/awra-frame-pool.h',
  'include/awra-1/awra/awra-texture-stream.h',
  'include/awra-1/awra/awra-volume-control.h',
  'include/awra-1/awra/awra-media-row.h',
  'include/awra-1/awra/awra-drag-preview.h',
  'include/awra-1/awra/awra-drop-zone.h',
)

awra_sources = files(
  'src/awra-init.c',
  'src/awra-enums.c',
  'src/core/awra-context.c',
  'src/core/awra-style-manager.c',
  'src/core/awra-style-role.c',
  'src/tokens/awra-token-set.c',
  'src/tokens/awra-typography.c',
  'src/layout/awra-layout.c',
  'src/material/awra-material.c',
  'src/material/awra-material-resolution.c',
  'src/material/awra-material-resolver.c',
  'src/platform/awra-diagnostics.c',
  'src/platform/awra-effect-coordinator.c',
  'src/platform/generic/awra-generic-effect-backend.c',
  'src/surface/awra-surface.c',
  'src/widgets/awra-button.c',
  'src/application/awra-window.c',
  'src/overlays/awra-popover.c',
  'src/overlays/awra-menu-button.c',
  'src/overlays/awra-context-menu.c',
  'src/layout/awra-card.c',
  'src/layout/awra-page.c',
  'src/layout/awra-page-header.c',
  'src/layout/awra-section.c',
  'src/layout/awra-toolbar.c',
  'src/layout/awra-form-row.c',
  'src/layout/awra-form.c',
  'src/layout/awra-state-views.c',
  'src/layout/awra-filter-bar.c',
  'src/layout/awra-master-detail.c',
  'src/layout/awra-detail-pane.c',
  'src/widgets/awra-data-column.c',
  'src/widgets/awra-data-view.c',
  'src/widgets/awra-selection-toolbar.c',
  'src/widgets/awra-data-components.c',
  'src/widgets/awra-advanced-inputs.c',
  'src/widgets/awra-choice-group.c',
  'src/widgets/awra-status-banner.c',
  'src/widgets/awra-skeleton.c',
  'src/layout/awra-header.c',
  'src/layout/awra-sidebar.c',
  'src/layout/awra-split-view.c',
  'src/layout/awra-tile-view.c',
  'src/layout/awra-responsive-bin.c',
  'src/layout/awra-edge-panel.c',
  'src/layout/awra-overlay-bar.c',
  'src/widgets/awra-controls.c',
  'src/widgets/awra-dropdown.c',
  'src/overlays/awra-dialog.c',
  'src/overlays/awra-command-palette.c',
  'src/overlays/awra-toast-overlay.c',
  'src/navigation/awra-navigation-view.c',
  'src/navigation/awra-breadcrumb-bar.c',
  'src/navigation/awra-path-bar.c',
  'src/navigation/awra-navigation-item.c',
  'src/motion/awra-motion.c',
  'src/motion/awra-transition-bin.c',
  'src/motion/awra-animation-target.c',
  'src/motion/awra-animation.c',
  'src/motion/awra-timed-animation.c',
  'src/motion/awra-spring-animation.c',
  'src/widgets/awra-widget-style.c',
  'src/widgets/awra-avatar.c',
  'src/widgets/awra-badge.c',
  'src/widgets/awra-progress-bar.c',
  'src/widgets/awra-segmented-control.c',
  'src/navigation/awra-tab-view.c',
  'src/widgets/awra-list-view.c',
  'src/widgets/awra-grid-view.c',
  'src/widgets/awra-zoom-controller.c',
  'src/widgets/awra-action-row.c',
  'src/platform/awra-inspector.c',
  'src/widgets/awra-key-codes.c',
  'src/widgets/awra-key-cap.c',
  'src/widgets/awra-keyboard-view.c',
  'src/widgets/awra-shortcut-capture.c',
  'src/widgets/awra-waveform-view.c',
  'src/widgets/awra-timeline.c',
  'src/widgets/awra-aspect-paintable.c',
  'src/widgets/awra-frame-pool.c',
  'src/widgets/awra-texture-stream.c',
  'src/widgets/awra-volume-control.c',
  'src/widgets/awra-media-row.c',
  'src/widgets/awra-drag-preview.c',
  'src/widgets/awra-drop-zone.c',
)

awra_resources = gnome.compile_resources(
  'awra-resources',
  'resources/awra.gresource.xml',
  source_dir: 'resources',
  c_name: 'awra',
)

awra_symbol_map = custom_target(
  'awra-symbol-map',
  input: public_headers,
  output: 'awra.map',
  command: [
    python,
    files('build-aux/generate-version-script.py'),
    '@OUTPUT@',
    '@INPUT@',
  ],
)

# ceil, floor and round: libm is a library of its own on Linux.
awra_deps = [gtk_dep, gio_dep, cc.find_library('m', required: false)]
if wayland_dep.found() and wayland_scanner.found()
  protocol_xml = files('protocols/ext-background-effect-v1.xml')
  protocol_header = custom_target(
    'ext-background-effect-v1-client-header',
    input: protocol_xml,
    output: 'ext-background-effect-v1-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
  protocol_code = custom_target(
    'ext-background-effect-v1-private-code',
    input: protocol_xml,
    output: 'ext-background-effect-v1-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
  awra_sources += files('src/platform/wayland/awra-wayland-effect-backend.c')
  awra_sources += [protocol_header, protocol_code]
  awra_deps += wayland_dep
endif
if host_machine.system() == 'windows'
  awra_sources += files(
    'src/platform/win32/awra-win32-effect-backend.c',
    'src/platform/win32/awra-win32-system.c',
    'src/platform/win32/awra-win32-util.c',
    'src/platform/win32/awra-win32-window.c',
  )
  awra_deps += [
    cc.find_library('dwmapi'),
    cc.find_library('advapi32'),
    cc.find_library('comctl32'),
  ]
endif

# PE has no symbol versioning: a module-definition file limits the DLL to
# the same public symbols instead (MinGW would otherwise export them all).
awra_link_args = []
awra_link_depends = []
awra_library_kwargs = {}
if host_machine.system() == 'windows'
  awra_module_defs = custom_target(
    'awra-module-defs',
    input: public_headers,
    output: 'awra.def',
    command: [
      python,
      files('build-aux/generate-version-script.py'),
      '--def',
      '@OUTPUT@',
      '@INPUT@',
    ],
  )
  awra_library_kwargs += {'vs_module_defs': awra_module_defs}
else
  awra_link_args += '-Wl,--version-script=' + meson.project_build_root() / 'awra.map'
  awra_link_depends += awra_symbol_map
endif

libawra = library(
  'awra-1',
  awra_sources,
  awra_resources,
  version: meson.project_version(),
  soversion: '1',
  dependencies: awra_deps,
  include_directories: [inc_public, inc_private],
  link_args: awra_link_args,
  link_depends: awra_link_depends,
  install: true,
  kwargs: awra_library_kwargs,
)

awra_dep = declare_dependency(
  link_with: libawra,
  include_directories: inc_public,
  dependencies: gtk_dep,
)

install_headers(public_headers, subdir: 'awra-1/awra')

pkgconfig.generate(
  libawra,
  filebase: 'awra-1',
  name: 'Awra',
  description: 'An opinionated GTK 4 desktop UI framework',
  subdirs: 'awra-1',
  requires: ['gtk4 >= 4.22.0'],
)

gir = []
awra_vapi = []
have_awra_vapi = false
if not get_option('introspection').disabled()
  gir_dep = dependency(
    'gobject-introspection-1.0',
    required: get_option('introspection'),
  )
  if gir_dep.found()
    gir = gnome.generate_gir(
      libawra,
      sources: public_headers,
      namespace: 'Awra',
      nsversion: '1.0',
      identifier_prefix: 'Awra',
      symbol_prefix: 'awra',
      includes: ['Gtk-4.0', 'Gdk-4.0', 'Gio-2.0'],
      header: 'awra/awra.h',
      export_packages: 'awra-1',
      install: true,
      extra_args: ['--warn-all', '--c-include=awra/awra.h'],
    )
    vapigen = find_program('vapigen', required: false)
    if vapigen.found()
      awra_vapi = gnome.generate_vapi(
        'awra-1',
        sources: gir[0],
        packages: ['gtk4'],
        install: true,
      )
      have_awra_vapi = true
    endif
  endif
endif

subdir('docs')
subdir('po')

meson.add_dist_script(
  python,
  files('build-aux/normalize-dist-mtime.py'),
)

if get_option('showcase')
  subdir('showcase')
  subdir('packaging')
endif
if get_option('examples')
  subdir('examples')
endif
if get_option('tests')
  subdir('tests')
endif
