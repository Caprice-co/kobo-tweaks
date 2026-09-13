# Kobo Tweaks Fork — Changelog & Diagnosis Notes

Fork of [redphx/kobo-tweaks](https://github.com/redphx/kobo-tweaks), based on commit
`07c1eafb9dd8f4311cbec0f1deaf15aa8dd3a9c8`. Tested on Kobo Libra 2 (4.38.23697) "Device One" and
Kobo Libra Colour (4.45.23697) "Device Two".

---

## 1. Header/footer spacer margin bug - Applicable to both Devices

**Symptom:** The gap between the header widget row and the book text was noticeably
smaller than the equivalent gap on the footer, with no combination of `settings.ini`
values able to close the difference.

**Diagnosis:** In `src/hooks/reading_view.cc`, `headerSpacerHeight` and
`footerSpacerHeight` were being passed to `QLayout::setContentsMargins(left, top,
right, bottom)` in the wrong argument positions — `headerSpacerHeight` was going into
the *top* slot (pushing the whole header down from the screen edge) instead of the
*bottom* slot (which would add space between the header and the text below it). The
footer had the equivalent mistake, reversed.

**Fix:**
```cpp
// Before
layout->setContentsMargins(0, readingSettings.headerSpacerHeight, 0, 0);
layout->setContentsMargins(0, 0, 0, readingSettings.footerSpacerHeight);

// After
layout->setContentsMargins(0, 0, 0, readingSettings.headerSpacerHeight);
layout->setContentsMargins(0, readingSettings.footerSpacerHeight, 0, 0);
```

Independently reported upstream as [issue #46](https://github.com/redphx/kobo-tweaks/issues/46).

---

## 2. Color-panel widget flicker / reposition on page turn - Applicable to Device Two (Separated Branch)

**Symptom:** On Kobo Libra Colour, header/footer widgets flashed and occasionally
shifted position on every page turn. Kobo Libra 2 (monochrome) was unaffected —
widgets stayed static and only the displayed text updated.

**Diagnosis:** `onPageChanged()` in the progress/page/time widgets called
`textLabel->setText(...)` unconditionally on every page turn, regardless of whether
the displayed value had actually changed. `QLabel::setText()` triggers
`updateGeometry()` internally even when the new text is identical to the old text,
forcing a geometry recompute of the whole widget zone. On monochrome e-ink this
recompute is visually imperceptible; on the Kaleido color panel it produces a visible
flash and can shift widget positions.

**Fix:** Added a change-detection guard before every `setText()` call, matching the
pattern already used by `TwElidedLabel::setFullText()` for the title widgets:
```cpp
QString newText = /* ...computed value... */;
if (textLabel->text() != newText) {
    textLabel->setText(newText);
}
```
Applied to: `chapter_progress_widget.h`, `chapter_page_widget.h`,
`chapter_time_widget.h`, `book_progress_widget.h`, `book_page_widget.h`,
`book_time_widget.h`. (`battery_widget.h` already had an equivalent guard and did not
need this change.)

**Result:** Remaining flicker/reposition was reduced to only the cases where the
displayed value's *text width* genuinely changes (e.g. page count going from 1 to 2
digits) — a real, expected layout change rather than a bug.

---

## 3. Fixed/minimum-width sizing to prevent digit-count reflow

**Goal:** Stop the remaining reflow from #2 by reserving enough width up front that
normal value changes (single digit → double digit, etc.) never change the widget's
footprint.

**Fix:** Added sizing to each widget's text label, based on the widest realistic
value for that field. Initially implemented with `setFixedWidth()`, later changed by
the user to `setMinimumWidth()` — same reflow protection, but allows the label to
shrink to fit shorter content instead of always reserving the full sample width,
removing unwanted dead space around shorter values. (Trade-off: with `setMinimumWidth`
there is no upper cap, so a value exceeding the sample string would grow the widget
instead of clipping — not expected to occur given the ranges chosen below.)

Final sizing samples in use:
| Widget | Sample string used for sizing |
|---|---|
| `book_page_widget.h` | `"9999 OF 9999"` |
| `chapter_page_widget.h` | `"999 OF 999"` |
| `chapter_time_widget.h` | `"99h 59m"` |
| `book_time_widget.h` | `"99h 59m"` |
| `chapter_progress_widget.h` | `"100%"` |
| `book_progress_widget.h` | `"100%"` |
| `battery_widget.h` (`levelLabel`) | `"100%"` |

Alignment: page-count widgets (`book_page_widget.h`, `chapter_page_widget.h`) use
`Qt::AlignLeft | Qt::AlignVCenter` so reserved slack trails after the text rather than
floating between the preceding separator and the number. Progress/time widgets use
`Qt::AlignCenter`.

Also investigated but **not needed**: battery icon size (`battery_%1.png`,
`battery_charging_%1.png`, `battery_charged.png`) was confirmed consistent at 26×26
across all states — no shift risk from charger plug/unplug.

---

## 4. Attempted title-centering fix — reverted, caused a crash

**Goal:** Stop the title widget (`TwElidedLabel`) from absorbing layout stretch space
meant for a fixed-width sibling (e.g. a page-count widget), which was pushing the
whole title+number group off-center.

**Attempted fix:** Added `setMaximumWidth()` (computed from current text width) plus
unconditional `updateGeometry()`/`update()` calls inside `TwElidedLabel::setFullText()`.

**Result: caused Nickel to crash on every book open** (blank page, then automatic
restart). **This change was reverted** — `setFullText()` restored to its original
form (guard, `QLabel::setText()`, visibility/separator sync only, no geometry calls
added).

**Root cause of the crash** (see section 5) turned out to be a related but distinct
issue — calling font-dependent code too early in a widget's lifecycle — which is why
the *next* fix (section 5) generalizes the lesson learned here.

**Not yet resolved:** true centering of a title+widget group within a `Left`/`Right`
zone. Established that `TwWidgetZonesContainer` only centers content placed in the
`Center` slot by design (`Qt::AlignCenter` is applied only to the center zone) — zones
assigned to `Left`/`Right` are intentionally edge-anchored. Moving the relevant widget
list to `HeaderCenter`/`FooterCenter` in `settings.ini` is the recommended way to get
true centering, rather than further code changes.

---

## 5. Crash on every book open (root cause + real fix)

**Symptom:** After adding the fixed/minimum-width sizing code (section 3) directly
inside each widget's constructor, Nickel crashed every time a book was opened — blank
screen, then automatic restart.

**Diagnosis (confirmed via device crash log, not inference):**
Captured a persistent syslog across the crash/restart cycle (`syslogd -O ... -S`),
which contained a `hindenburg` crash dump:
```
signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 00000014
#00 .../libtweaks.so: TwWidgetZone::setupWidgets(...)+0x9cb
#01 .../libtweaks.so: TwWidgetZonesContainer::setupZones(...)+0x71a
#02 .../libtweaks.so: ReadingViewHook::constructor(QWidget*)::{lambda}+0x132
...
#05 .../libnickel.so.1.0.0: ReadingView::readerIsDoneLoading()+0x104
```
Disassembling `libtweaks.so` around the faulting offset confirmed the crash was
inside the newly-added `QFontMetrics fm(textLabel->font()); ... setFixedWidth(...)`
code, called from each widget's constructor.

**Root cause:** `QWidget::font()` is not a simple field read — it resolves the
effective font via Qt's internal widget-private data (parent/stylesheet cascade).
Calling it from inside a widget's own constructor, before the widget is attached to
its parent hierarchy or styled, means that internal data isn't populated yet, and
the resulting pointer chase dereferences invalid memory (null pointer + small offset,
matching the `fault addr 00000014`).

**Fix:** Moved all font-metrics-based sizing out of each widget's constructor and
into a one-time, lazily-guarded block at the top of `onPageChanged()`:
```cpp
bool widthInitialized = false;
...
void onPageChanged() override {
    if (!widthInitialized) {
        QFontMetrics fm(textLabel->font());
        int maxWidth = fm.width(QStringLiteral("..."));
        textLabel->setMinimumWidth(maxWidth); // later changed from setFixedWidth
        textLabel->setAlignment(...);
        widthInitialized = true;
    }
    // ...existing logic unchanged
}
```
By the time `onPageChanged()` first runs, the widget is fully constructed and part of
the live widget tree, so `font()` resolves safely.

For `battery_widget.h` specifically — whose constructor calls `updateLevel()`
synchronously rather than waiting for a page-turn event — the same lazy-init block
was placed in an overridden `event()` handler, gated on `QEvent::Show`, which Qt
guarantees fires only once the widget is fully constructed and visible.

**Confirmed fixed** on-device: books open normally, and the flicker/reposition fixes
from sections 2–3 remain in effect.

---

## Files touched, final state

- `src/hooks/reading_view.cc` — margin argument order fix (section 1)
- `src/widgets/chapter_progress_widget.h` — guard + lazy min-width (sections 2, 3, 5)
- `src/widgets/chapter_page_widget.h` — guard + lazy min-width (sections 2, 3, 5)
- `src/widgets/chapter_time_widget.h` — guard + lazy min-width (sections 2, 3, 5)
- `src/widgets/book_progress_widget.h` — guard + lazy min-width (sections 2, 3, 5)
- `src/widgets/book_page_widget.h` — guard + lazy min-width (sections 2, 3, 5)
- `src/widgets/book_time_widget.h` — guard + lazy min-width (sections 2, 3, 5)
- `src/widgets/battery_widget.h` — lazy min-width via `QEvent::Show` (section 5)
- `src/widgets/base/elided_label.h` — attempted change reverted to original (section 4)
