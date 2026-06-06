# Project Handoff: CYD Snake Game with 8BitDo Zero 2 Remote

You are taking over development of an ESP32-based Snake Game running on a Cheap Yellow Display (CYD / ESP32-2432S028).

Read this entire document before proposing changes.

---

# Current Status

We now have TWO projects:

1. `snakegame`

   * Existing playable Snake game.
   * Uses touchscreen controls.
   * Current screen layout includes touch buttons.
   * This is the target application.

2. `snakegame_controller_test`

   * Dedicated Bluetooth controller test application.
   * Used to develop and validate Bluepad32 integration.
   * Controller connectivity is now proven.

The controller test project is the source of truth for controller integration.

---

# Hardware

Display:

* Cheap Yellow Display (ESP32-2432S028)
* 320x240 landscape

Controller:

* 8BitDo Zero 2

Bluetooth stack:

* Bluepad32
* ESP-IDF integration
* Not PlatformIO lib_deps

---

# Controller Status

Controller connection is working.

Known-good startup mode:

B + Start
(DInput / Android mode)

Other startup modes may exist but are no longer important.

We are standardizing on:

8BitDo Zero 2
B+Start
DInput mode

---

# Proven Controller Behavior

Directional controls arrive as analog axes.

Observed values:

Neutral:
axis_x ~= -4
axis_y ~= -4

UP:
axis_y ~= -512

DOWN:
axis_y ~= 508

LEFT:
axis_x ~= -512

RIGHT:
axis_x ~= 508

Use deadzone:

abs(axis) < 200

to indicate neutral.

Direction mapping:

axis_y < -200 -> UP
axis_y > 200  -> DOWN
axis_x < -200 -> LEFT
axis_x > 200  -> RIGHT

If both axes are active:
choose the axis with the larger magnitude.

---

# Button Status

Working:

A
B

Observed:

A/B generate reliable events.

Not currently needed:

X
Y
L
R

Do not spend time debugging these.

They may be revisited later.

---

# Semantic Commands

Controller layer should expose:

NONE
UP
DOWN
LEFT
RIGHT
ACTION_1
ACTION_2
START
SELECT

Recommended mapping:

UP/DOWN/LEFT/RIGHT -> snake direction

ACTION_1 -> pause/resume

ACTION_2 -> start/restart

START/SELECT -> reserved for future menus

---

# Controller Integration Lessons Learned

Several days were spent debugging Bluetooth.

Bluetooth is solved.

Do not redesign Bluetooth.

Do not redesign Bluepad32.

Do not replace the controller architecture.

Do not switch libraries.

The working controller test implementation should simply be imported into Snake.

---

# Current Design Goal

Replace touchscreen gameplay controls with remote control.

Touchscreen remains available as fallback.

---

# Desired Startup Behavior

On startup:

Display splash/status screen.

Wait:

max(10 seconds, time until controller connects)

More specifically:

* Wait up to 10 seconds for controller.
* If controller connects sooner:
  continue immediately.
* If controller never connects:
  continue in touchscreen mode.

Do NOT block forever waiting for Bluetooth.

---

# Desired Gameplay Modes

Mode A:

Remote connected

Use remote for gameplay.

Mode B:

No remote

Use touchscreen fallback.

Both modes should continue to function.

---

# Screen Redesign

Current design contains touchscreen direction buttons.

Those should be removed.

New layout:

Landscape 320x240

Top status banner:

height ~= 28 pixels

Remaining screen:

game board

Layout:

Y=0..27

Status banner

Y=28..239

Game board

---

# Banner Contents

Keep minimal.

Suggested:

Score
State
Bluetooth status

Example:

Score: 125

PLAY

BT:OK

Bluetooth indicator should be unobtrusive.

Examples:

BT:OK
BT:--

Small icon
Green dot

Any of these are acceptable.

Do not create a large Bluetooth UI.

---

# Game Board

Maximize playable area.

No on-screen direction controls.

No large buttons.

No large status panels.

The board should dominate the display.

---

# Touchscreen Fallback

Retain an invisible touch region.

Recommended:

Upper-left corner.

Uses:

Start
Resume
Emergency menu access

No visible button required.

---

# Memory / Persistence

Recently acquired:

ESP32 memory support.

This is NOT part of the current milestone.

Do not implement:

high scores
saved settings
profiles
persistent state

yet.

That work comes later.

---

# Immediate Milestone

Complete controller integration into Snake.

Specifically:

1. Import controller module.
2. Remove touchscreen gameplay buttons.
3. Redesign display around banner + board.
4. Support:

   * remote control
   * touchscreen fallback
5. Preserve working Bluetooth behavior.

---

# What NOT To Do

Do NOT:

* Replace Bluepad32.
* Replace controller architecture.
* Revisit controller startup modes.
* Debug X/Y/L/R buttons.
* Add memory features.
* Add settings screens.
* Add complex menus.
* Redesign the game rules.

---

# Expected Deliverables

Before modifying code:

1. Analyze current snakegame architecture.
2. Identify:

   * game state machine
   * rendering path
   * input path
   * touch handling
   * display layout

Then:

3. Produce a migration plan.
4. Implement incrementally.
5. Explain every file modified.
6. Provide complete code for modified files.

Primary objective:

Create a clean, playable Snake game using the 8BitDo Zero 2 remote while preserving touchscreen fallback.

