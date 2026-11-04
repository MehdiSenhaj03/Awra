/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <awra/awra-version.h>
#include <awra/awra-enums.h>
#include <awra/awra-token-set.h>
#include <awra/awra-typography.h>
#include <awra/awra-layout.h>
#include <awra/awra-style-manager.h>
#include <awra/awra-style-role.h>
#include <awra/awra-material.h>
#include <awra/awra-material-resolution.h>
#include <awra/awra-diagnostics.h>
#include <awra/awra-context.h>
#include <awra/awra-surface.h>
#include <awra/awra-button.h>
#include <awra/awra-window.h>
#include <awra/awra-popover.h>
#include <awra/awra-menu-button.h>
#include <awra/awra-context-menu.h>
#include <awra/awra-card.h>
#include <awra/awra-page.h>
#include <awra/awra-page-header.h>
#include <awra/awra-section.h>
#include <awra/awra-toolbar.h>
#include <awra/awra-form-row.h>
#include <awra/awra-form.h>
#include <awra/awra-empty-state.h>
#include <awra/awra-loading-state.h>
#include <awra/awra-error-state.h>
#include <awra/awra-filter-bar.h>
#include <awra/awra-master-detail.h>
#include <awra/awra-detail-pane.h>
#include <awra/awra-data-column.h>
#include <awra/awra-data-view.h>
#include <awra/awra-selection-toolbar.h>
#include <awra/awra-metric-row.h>
#include <awra/awra-stat-bar.h>
#include <awra/awra-badge-group.h>
#include <awra/awra-metadata-group.h>
#include <awra/awra-text-area.h>
#include <awra/awra-password-entry.h>
#include <awra/awra-tag-entry.h>
#include <awra/awra-choice-group.h>
#include <awra/awra-status-banner.h>
#include <awra/awra-skeleton.h>
#include <awra/awra-header.h>
#include <awra/awra-sidebar.h>
#include <awra/awra-split-view.h>
#include <awra/awra-responsive-bin.h>
#include <awra/awra-edge-panel.h>
#include <awra/awra-overlay-bar.h>
#include <awra/awra-toggle-button.h>
#include <awra/awra-entry.h>
#include <awra/awra-search-entry.h>
#include <awra/awra-switch.h>
#include <awra/awra-slider.h>
#include <awra/awra-check-button.h>
#include <awra/awra-spin-button.h>
#include <awra/awra-dropdown.h>
#include <awra/awra-dialog.h>
#include <awra/awra-command-palette.h>
#include <awra/awra-toast-overlay.h>
#include <awra/awra-navigation-view.h>
#include <awra/awra-breadcrumb-item.h>
#include <awra/awra-breadcrumb-bar.h>
#include <awra/awra-path-bar.h>
#include <awra/awra-navigation-item.h>
#include <awra/awra-motion.h>
#include <awra/awra-badge.h>
#include <awra/awra-progress-bar.h>
#include <awra/awra-segmented-control.h>
#include <awra/awra-tab-view.h>
#include <awra/awra-list-view.h>
#include <awra/awra-grid-view.h>
#include <awra/awra-zoom-controller.h>
#include <awra/awra-action-row.h>
#include <awra/awra-inspector.h>
#include <awra/awra-key-cap.h>
#include <awra/awra-keyboard-view.h>
#include <awra/awra-shortcut-capture.h>
#include <awra/awra-waveform-view.h>
#include <awra/awra-timeline.h>
#include <awra/awra-transition-bin.h>
#include <awra/awra-animation-target.h>
#include <awra/awra-animation.h>
#include <awra/awra-timed-animation.h>
#include <awra/awra-spring-animation.h>
#include <awra/awra-avatar.h>
#include <awra/awra-rail-item.h>
#include <awra/awra-rail-group.h>
#include <awra/awra-rail.h>
#include <awra/awra-tile-view.h>
#include <awra/awra-aspect-paintable.h>
#include <awra/awra-frame-pool.h>
#include <awra/awra-texture-stream.h>
#include <awra/awra-volume-control.h>
#include <awra/awra-media-row.h>
#include <awra/awra-drag-preview.h>
#include <awra/awra-drop-zone.h>

G_BEGIN_DECLS

void awra_init (void);
void awra_widget_set_tooltip_text (GtkWidget  *widget,
                                   const char *text);

G_END_DECLS
