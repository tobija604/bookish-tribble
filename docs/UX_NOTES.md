# UX division of labor: touchscreen vs. web dashboard

The specification gives the AMOLED touchscreen a flat 12-item menu (section
4) and the web dashboard a much larger ~20-item menu with full CRUD editing
capability (section 25). This firmware leans into that asymmetry on
purpose rather than trying to replicate every web admin feature on a
466×466 round touchscreen:

- **Touchscreen (`src/display/screens/`)**: fast, glanceable status for
  every one of the 12 spec'd menu items, plus the two genuinely
  latency-sensitive/physical actions — UNLOCK (auth-gated: RFID tap or
  security code, never bare touch — spec 56) and LOCK. RFID's on-device
  role is limited to the "SCAN CARD" capture step of adding a card; the
  card still needs a user assigned to it, which happens on the web UI.
- **Web dashboard (`data/`)**: where all editing happens — users, card
  assignment, schedules, servo calibration, automation rules, appearance,
  backups, firmware updates, factory reset (with a confirmation flow that
  mirrors the physical BUTTON2-hold + AMOLED-confirm combo).

This means `InfoScreen.h`'s 9 read-only screens (STATUS, USERS, EVENTS,
SETTINGS, NETWORK, SECURITY, DEVICE, DIAGNOSTICS, ABOUT) intentionally show
a live summary and point the user at the web UI for anything that needs a
form, rather than trying to cram a full CRUD table + soft keyboard onto a
round touchscreen. If a future revision wants on-device editing for one of
these (e.g. disabling a user without leaving the couch), the natural next
step is adding a dedicated screen for just that one action, following the
same `ScreenManager`/`InfoScreen` pattern already in place — not rebuilding
the whole menu system.
