# SPDX-License-Identifier: LGPL-2.1-or-later

awra_test_internal = static_library(
  'awra-test-internal',
  awra_sources,
  awra_resources,
  dependencies: awra_deps,
  include_directories: [inc_public, inc_private],
)
awra_test_internal_dep = declare_dependency(
  link_whole: awra_test_internal,
  include_directories: [inc_public, inc_private],
  dependencies: awra_deps,
)

unit_material = executable(
  'test-material',
  'unit/test-material.c',
  dependencies: [awra_test_internal_dep, cc.find_library('m')],
  include_directories: inc_private,
)
test('material-matrix', unit_material)

external_c = executable(
  'external-smoke-c',
  'integration/external-smoke.c',
  dependencies: awra_dep,
)
test('external-c-consumer', external_c)

test(
  'public-symbols',
  python,
  args: [
    files('api/test-public-symbols.py'),
    libawra,
    files('api/phase-a-contract.txt'),
    public_headers,
  ],
  depends: [libawra, awra_symbol_map],
)

test(
  'public-consumer-boundary',
  python,
  args: [
    files('../build-aux/check-public-consumers.py'),
    meson.project_source_root() / 'showcase',
    meson.project_source_root() / 'docs/guides',
      meson.project_source_root() / 'tests/consumers',
      meson.project_source_root() / 'bindings/rust',
      meson.project_source_root() / 'examples',
  ],
)

test(
  'flatpak-manifest-contract',
  python,
  args: [
    files('api/test-flatpak-manifest.py'),
    files('../packaging/org.awra.Showcase.Devel.json'),
  ],
)

test(
  'rust-safe-projection',
  python,
  args: [
    files('api/test-rust-projection.py'),
    meson.project_source_root() / 'bindings/rust/awra',
  ],
)

widget_basics = executable(
  'test-widgets',
  'widget/test-widgets.c',
  dependencies: awra_test_internal_dep,
  include_directories: inc_private,
)
test('widget-basics', widget_basics, is_parallel: false)

media_input = executable(
  'test-media-input',
  'widget/test-media-input.c',
  dependencies: [awra_test_internal_dep, cc.find_library('m')],
  include_directories: inc_private,
)
test(
  'media-input-widgets',
  media_input,
  env: ['G_DEBUG=fatal-warnings'],
  is_parallel: false,
)

accessibility_audit = executable(
  'test-accessibility',
  'widget/test-accessibility.c',
  dependencies: awra_test_internal_dep,
  include_directories: inc_private,
)
test(
  'accessibility-rtl',
  accessibility_audit,
  env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=test'],
  is_parallel: false,
)

render_budget = executable(
  'test-render-budget',
  'performance/test-render-budget.c',
  dependencies: awra_test_internal_dep,
  include_directories: inc_private,
)
test(
  'render-budget',
  render_budget,
  env: ['G_DEBUG=fatal-warnings'],
  timeout: 45,
  is_parallel: false,
)

key_cap_resources = executable(
  'test-key-cap-resources',
  'performance/test-key-cap-resources.c',
  dependencies: awra_dep,
)

test(
  'key-cap-resources',
  key_cap_resources,
  env: ['GSK_RENDERER=cairo', 'GTK_A11Y=none'],
  timeout: 30,
  is_parallel: false,
)

fallback_context = executable(
  'test-fallback-context',
  'integration/test-fallback-context.c',
  dependencies: awra_dep,
)
test(
  'generic-fallback',
  fallback_context,
  env: ['AWRA_FORCE_FALLBACK=1'],
)

native_lifecycle = executable(
  'test-native-lifecycle',
  'integration/test-native-lifecycle.c',
  dependencies: awra_dep,
)
test('native-lifecycle', native_lifecycle, is_parallel: false)

visual_render = executable(
  'test-render',
  'visual/test-render.c',
  dependencies: awra_dep,
)
test(
  'visual-render',
  visual_render,
  env: ['AWRA_FORCE_FALLBACK=1'],
  is_parallel: false,
)

style_isolation = executable(
  'test-style-isolation',
  'visual/test-style-isolation.c',
  dependencies: awra_dep,
)

page_invariants = executable(
  'test-page-invariants',
  'visual/test-page-invariants.c',
  dependencies: awra_dep,
)
test(
  'page-invariants',
  page_invariants,
  env: [
    'G_DEBUG=fatal-warnings',
    'GTK_A11Y=none',
  ],
  timeout: 45,
  is_parallel: false,
)

if gir.length() > 0
  test(
    'gir-contract',
    python,
    args: [files('api/test-gir-contract.py'), gir[0]],
    depends: gir,
  )
  # The current API, exactly; and every 1.0.0 API still there, unchanged.
  test(
    'api-baseline',
    python,
    args: [
      files('api/test-api-baseline.py'),
      gir[0],
      files('api/awra-api.json'),
    ],
    depends: gir,
  )
  test(
    'api-1.0-compatible',
    python,
    args: [
      files('api/test-api-baseline.py'),
      '--compatible',
      gir[0],
      files('api/awra-1.0-api.json'),
    ],
    depends: gir,
  )
  test(
    'python-gi-consumer',
    python,
    args: files('integration/external-smoke.py'),
    env: [
      'GI_TYPELIB_PATH=' + meson.project_build_root(),
      'LD_LIBRARY_PATH=' + meson.project_build_root(),
    ],
    depends: gir,
    is_parallel: false,
  )

  if vala_available and have_awra_vapi
    external_vala = executable(
      'external-smoke-vala',
      'integration/external-smoke.vala',
      dependencies: [awra_dep, awra_vapi],
      c_args: ['-Wno-unused-parameter'],
    )
    test('external-vala-consumer', external_vala)
  endif

  cargo = find_program('cargo', required: false)
  if cargo.found() and get_option('developer')
    rust_manifest = files('consumers/rust-experimental/Cargo.toml')
    test(
      'rust-experimental-consumer',
      cargo,
      args: [
        'run',
        '--quiet',
        '--offline',
        '--manifest-path', rust_manifest,
      ],
      env: [
        'AWRA_BUILD_DIR=' + meson.project_build_root(),
        'AWRA_GIR_PATH=' + meson.project_build_root() / 'Awra-1.0.gir',
        'CARGO_TARGET_DIR=' + meson.project_build_root() / 'rust-consumer-target',
        'LD_LIBRARY_PATH=' + meson.project_build_root(),
      ],
      depends: gir,
      timeout: 240,
      is_parallel: false,
    )
    rust_bindings_manifest = files('../bindings/rust/Cargo.toml')
    rust_bindings_env = [
      'PKG_CONFIG_PATH=' + meson.project_build_root() / 'meson-uninstalled',
      'LD_LIBRARY_PATH=' + meson.project_build_root(),
      'CARGO_TARGET_DIR=' + meson.project_build_root() / 'rust-bindings-target',
    ]
    test(
      'rust-generated-bindings',
      cargo,
      args: [
        'test', '--quiet', '--offline',
        '--manifest-path', rust_bindings_manifest,
      ],
      env: rust_bindings_env,
      depends: gir,
      timeout: 300,
      is_parallel: false,
    )
    test(
      'rust-generated-example',
      cargo,
      args: [
        'run', '--quiet', '--offline',
        '--manifest-path', rust_bindings_manifest,
        '-p', 'awra-rust-example',
      ],
      env: rust_bindings_env,
      depends: gir,
      timeout: 300,
      is_parallel: false,
    )
    test(
      'rust-1.0-api',
      cargo,
      args: [
        'check', '--quiet', '--offline',
        '--manifest-path', rust_bindings_manifest,
        '-p', 'awra', '--no-default-features',
      ],
      env: rust_bindings_env,
      depends: gir,
      timeout: 300,
      is_parallel: false,
    )
  endif
endif
test(
  'style-isolation',
  style_isolation,
  env: [
    'G_DEBUG=fatal-warnings',
    'GTK_A11Y=none',
  ],
  timeout: 45,
  is_parallel: false,
)

timeline_test = executable(
  'test-timeline', 'widget/test-timeline.c',
  dependencies: awra_dep,
)
test('timeline', timeline_test, env: ['G_DEBUG=fatal-warnings'], is_parallel: false)

overlay_bar_test = executable('test-overlay-bar', 'widget/test-overlay-bar.c', dependencies: awra_dep)
test('overlay-bar-workflows', overlay_bar_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)

frame_pool_test = executable('test-frame-pool', 'widget/test-frame-pool.c', dependencies: awra_dep)
test('frame-pool', frame_pool_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)

window_frame_test = executable('test-window-frame', 'integration/test-window-frame.c', dependencies: awra_dep)
test('window-frame-states', window_frame_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)

task_row_test = executable('test-task-row', 'widget/test-task-row.c', dependencies: awra_dep)
test('task-row', task_row_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
transition_bin_test = executable('test-transition-bin', 'widget/test-transition-bin.c', dependencies: awra_dep)
test('transition-bin', transition_bin_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
tile_view_test = executable('test-tile-view', 'widget/test-tile-view.c', dependencies: awra_dep)
test('tile-view', tile_view_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
animation_test = executable('test-animation', 'widget/test-animation.c', dependencies: [awra_dep, cc.find_library('m', required: false)])
test('animation', animation_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
avatar_test = executable('test-avatar', 'widget/test-avatar.c', dependencies: awra_dep)
test('avatar', avatar_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
rail_test = executable('test-rail', 'widget/test-rail.c', dependencies: awra_dep)
test('rail', rail_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
typing_indicator_test = executable('test-typing-indicator', 'widget/test-typing-indicator.c', dependencies: awra_dep)
test('typing-indicator', typing_indicator_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
rich_label_test = executable('test-rich-label', 'widget/test-rich-label.c', dependencies: awra_dep)
test('rich-label', rich_label_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
chip_test = executable('test-chip', 'widget/test-chip.c', dependencies: awra_dep)
test('chip', chip_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
conversation_view_test = executable('test-conversation-view', 'widget/test-conversation-view.c', dependencies: awra_dep)
test('conversation-view', conversation_view_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
composer_test = executable('test-composer', 'widget/test-composer.c', dependencies: awra_dep)
test('composer', composer_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
completion_popover_test = executable('test-completion-popover', 'widget/test-completion-popover.c', dependencies: awra_dep)
test('completion-popover', completion_popover_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
popover_menu_test = executable('test-popover-menu', 'widget/test-popover-menu.c', dependencies: awra_dep)
test('popover-menu', popover_menu_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
divider_test = executable('test-divider', 'widget/test-divider.c', dependencies: awra_dep)
test('divider', divider_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
emoji_picker_test = executable('test-emoji-picker', 'widget/test-emoji-picker.c', dependencies: awra_dep)
test('emoji-picker', emoji_picker_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
drag_preview_test = executable('test-drag-preview', 'widget/test-drag-preview.c', dependencies: awra_dep)
test('drag-preview', drag_preview_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
path_bar_test = executable('test-path-bar', 'widget/test-path-bar.c', dependencies: awra_dep)
test('path-bar', path_bar_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)

grid_test = executable('test-grid-view', 'widget/test-grid-view.c', dependencies: awra_dep)
test('grid-view-collections', grid_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)

zoom_test = executable('test-zoom-controller', 'widget/test-zoom-controller.c', dependencies: awra_dep)
test('zoom-controller', zoom_test, env: ['G_DEBUG=fatal-warnings', 'GTK_A11Y=none'], is_parallel: false)
backdrop_test = executable('test-backdrop', 'visual/test-backdrop.c', dependencies: awra_dep)
test('local-backdrop', backdrop_test)
