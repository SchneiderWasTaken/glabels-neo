# Work plan, file ownership and API contracts

Read `SPEC.md` first. This file stops agents from colliding.

## Branches

The integration branch is `feat/ribbon-ui` (this branch; it holds the docs only until work
lands). Every agent works in **its own worktree on its own branch** forked from
`feat/ribbon-ui` and never touches another agent's branch or the main checkout:

| Agent | Branch |
|---|---|
| Gemini 4 Neon | `feat/ribbon-ui-core` |
| Claude Opus 5.5 | `feat/ribbon-ui-theme` |

The orchestrator merges both into `feat/ribbon-ui`. Commit small and often (one logical
change per commit, conventional prefix `feat(ribbon):`, `feat(theme):`, `test(ribbon):`,
`docs(ribbon):`). **Do not push. Do not open pull requests.**

## File ownership (edit only your own; ask in QUESTIONS.md for anything else)

| Path | Owner |
|---|---|
| `glabels/theme/**`, `glabels/icons/neo/**`, `glabels/AppearanceModel.*`, dialog-level QSS, `THIRD-PARTY-NOTICES.md`, `third-party-licenses/**`, `glabels/unit_tests/TestTheme*` | **Opus** |
| `glabels/ribbon/**`, `glabels/MainWindow.*`, `glabels/ObjectEditor.*`, `glabels/FillView.*`, `glabels/MergeView.*`, `glabels/VariablesView.*`, `glabels/PrintView.*`, `glabels/PropertiesView.*`, `glabels/DeployView.*`, `glabels/StartupView.*`, `glabels/ui/*View.ui`, `glabels/ui/ObjectEditor.ui`, `glabels/unit_tests/TestRibbon*` | **Gemini** |
| `glabels/CMakeLists.txt`, `glabels/icons.qrc` | **Both, append-only**: each adds lines inside its own marked block (`# --- ribbon-ui: theme ---` and `# --- ribbon-ui: core ---`). Never reorder existing lines. |
| `docs/ribbon-ui/PARITY.md`, `QUESTIONS.md` | Both (append) |
| everything else (model, backends, glbarcode, updater, relay, CI) | nobody |

## Interface contract (WP1 delivers; WP2 to WP5 consume)

Opus implements exactly this header as `glabels/theme/Theme.hpp`. Gemini codes against it and,
**only on its own branch**, may add a temporary stub of the same header marked
`// TEMP STUB: replaced by feat/ribbon-ui-theme`. At merge the real one wins.

```cpp
#pragma once
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QObject>
#include <QString>

namespace glabels::theme
{
        enum class Mode     { Light, Dark };
        enum class IconRole { Normal, Accent, OnGradient, Disabled };

        struct Palette
        {
                QColor bg, chrome, ribbon, work, panel, line, line2;
                QColor ink, ink2, ink3, accent, tint, hover, pressed;
                QColor ctx, ctxLine, ok, okTint, warn;
                QColor gradA, gradB, gradC, onGrad;   // 135 degree gradient A > B > C
        };

        struct Metrics   // all in device-independent pixels, section 4.3 of SPEC.md
        {
                static constexpr int tabStrip = 42, tab = 34, tabActive = 36, ribbon = 118;
                static constexpr int largeW = 58, largeH = 78, smallH = 26, iconBtn = 28;
                static constexpr int field = 26, groupName = 22, groupPad = 8;
                static constexpr int contentPanel = 300, status = 28, ruler = 22;
        };

        class Theme : public QObject
        {
                Q_OBJECT
        public:
                static Theme& instance();
                Mode            mode() const;                 // resolved (System already resolved)
                const Palette&  palette() const;
                QFont           uiFont( int px = 12, int weight = 400 ) const;
                QFont           monoFont( int px = 12 ) const;
                QIcon           icon( const QString& name, IconRole role = IconRole::Normal ) const;
                QString         styleSheet() const;           // the whole app stylesheet
                void            applyToApplication();         // sets palette + stylesheet; emits changed()
        signals:
                void changed();                               // mode or palette changed: repaint custom widgets
        };
}
```

Custom-painted ribbon widgets must read colours from `Theme::instance().palette()` and
repaint on `changed()`. Widgets that are plain Qt widgets get their look from the stylesheet
(use dynamic properties such as `setProperty("role","ribbonLarge")` and match them in QSS;
the QSS owner will add the selectors, so list every property you introduce in
`docs/ribbon-ui/QSS-HOOKS.md`).

## Order of work

1. **Opus**: WP7 build bootstrap first (so everyone can compile), then WP1, then WP6.
2. **Gemini**: WP2, WP3, WP4, WP5 in that order, committing after each. Rebase onto the
   latest `feat/ribbon-ui` when the orchestrator tells you WP1 landed.
3. Orchestrator merges; Opus reviews WP2 to WP5 against SPEC.md section 10.

## Done means

SPEC.md section 10 fully ticked in `PARITY.md`, section 13 verification performed with real
logs quoted in the final report, all screens compared with the mockups in light and dark.
