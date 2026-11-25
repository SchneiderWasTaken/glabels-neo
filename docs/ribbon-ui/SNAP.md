# Snapping and guides (editor)

Owner request: adding elements should be easy, and elements should snap to the edges of
other items, to the horizontal and vertical centre, and to the **safe area** (the inside edge
of the label's margin, never the outer edge of the label).

## Targets (per axis, independent)

| Target | X positions | Y positions | Toggle |
|---|---|---|---|
| **Objects** | left, centre, right of every other object | top, middle, bottom of every other object | Objects |
| **Centre** | label centre x (`w/2`) | label centre y (`h/2`) | Centre |
| **Safe area** | inside left (`margin`), inside right (`w - margin`) | inside top, inside bottom | Safe area |

- The safe area is the label's margin from the template frame (the thin inset outline the
  canvas already draws with Markup). Use the frame's margin as the inset on all four sides of
  the label's bounding rectangle; for round, elliptical and CD labels use the same inset of
  the bounding rectangle. If the margin is 0 the safe-area edges coincide with the label edge.
- The label's **outer edge is not a target**.
- Objects being moved or resized are never targets for themselves. With several objects
  selected the moving rectangle is the selection's bounding box. Rotated objects and lines use
  their bounding box.

## When snapping applies

1. **Moving** selected objects by dragging: the moving rectangle's left, centre and right (X)
   and top, middle and bottom (Y) are the *sources*; each axis snaps to the nearest target
   within tolerance, independently.
2. **Resizing** with a handle: only the edge(s) being dragged are sources (and the centre
   line is not a source while resizing).
3. **Creating** an object by dragging with a tool: the corner under the cursor is the source
   for X and Y; the anchor corner is fixed. A plain click-create (default size) places the
   object where you click and the object's centre snaps to targets within tolerance.
4. Tolerance is **6 screen pixels** (convert to label units with the current scale, so it
   feels the same at every zoom). If two targets are within tolerance the nearest wins; ties
   prefer Centre, then Safe area, then Objects.
5. Hold **Alt** while dragging to bypass snapping for that gesture (if Alt already has a
   meaning in `LabelEditor`, use Ctrl+Alt and say so in QUESTIONS.md).
6. Snapping changes the position only; it never changes undo behaviour (still one checkpoint
   per gesture). Snapped positions are exact (no rounding to the display unit).
7. Keyboard nudging is unchanged and does not snap.

## Guide feedback (while a snap is active)

- A **1 px line across the whole label** at each snapped target, colour `palette().accent`.
  Centre guides are dashed (4 on, 3 off); object-edge and safe-area guides are solid.
- Safe-area guides also tint the existing margin outline with `accent` for the duration.
- A small caption near the cursor (`palette().panel` background, `ink` text, 12 px, 6 px
  padding, 6 px radius, offset 14 px right and below the cursor) names the target:
  "Centre", "Safe area", "Edge of <object type>", "Centre of <object type>"; when both axes
  snap, show two lines. It disappears when the gesture ends.
- Guides and caption are drawn on top of the canvas in `LabelEditor`, repaint on
  `Theme::changed()`, and are never printed or exported.

## Controls

- **View tab, new group "Snap"** (between Show and Zoom): three checkable small buttons
  "Objects", "Centre", "Safe area" (each with a status tip), all **on by default**, plus a
  caption line "Hold Alt to bypass". Icons: `magnet` (Objects), `crosshair` (Centre),
  `square-dashed` (Safe area), from the Lucide set in `glabels/icons/neo/` (add them if
  missing; the Lucide ISC notice already exists).
- State is persisted with the other view settings (`model::Settings`, keys
  `View/snapObjects`, `View/snapCenter`, `View/snapSafeArea`) and shared by all windows.
- The Home tab is unchanged.

## Code layout

- `glabels/SnapEngine.{hpp,cpp}`: **display-free**, pure functions, no Qt widgets. Input:
  moving rectangle (or the edges being dragged), a list of target rectangles, label size,
  safe-area inset, enabled flags, tolerance in label units. Output: delta x, delta y and the
  list of active guides (orientation, position, kind, description). Unit-tested in
  `glabels/unit_tests/TestSnap.*` (moves, resizes by each handle, create, tolerance edges,
  tie-breaking, disabled toggles, multi-selection box, zero margin, rotated bounds).
- `LabelEditor` calls the engine from its move, resize and create paths and paints guides.
- Wording follows the brand rules: sentence case, no emoji.

## Out of scope (for now)

Draggable guides pulled from the rulers, snapping to the grid, distribute/spacing guides.
