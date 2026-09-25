# omatimer

A native Qt6 rewrite of papertimer. One binary, no runtime, no browser.

## Build

Needs `qt6-base`. Sounds use `pw-play` from `pipewire` (or `paplay`);
without either the timer runs silently. `ttf-ia-writer` supplies the font.

```sh
qmake6 omatimer.pro -o Makefile
make -j$(nproc)
./omatimer
```

## Use

```sh
omatimer              # opens with the last duration you used
omatimer 25m          # opens preloaded with 25 minutes
omatimer 1h30m --start   # opens and starts counting immediately
```

Durations accept plain seconds (`90`) or units (`1h 30m 10s`, `1.5m`), and
the display counts down in the same notation — `1h5m30s`, `2m`, `45s` — so
whatever is on screen can be typed straight back in. Anything under a minute
shows as plain seconds. The number shrinks to fit when the units make it long.

| Key | Action |
|-----|--------|
| Enter / Space | start or stop |
| R | reset to the saved duration |
| B | swap the dark and light background |
| Ctrl+M | mute |
| Esc | quit |

Shortcuts are case-insensitive and work while the duration box has focus;
`h`, `m`, `s`, digits and `.` still type normally, which is why mute moved to
Ctrl+M (a plain `m` is part of `25m`).

## Colors

Colors come from the current Omarchy theme
(`~/.local/state/omarchy/current/theme/colors.toml`), the same file omacalc
and omawrite read, and re-tint live when you switch themes. The theme gives
one background/foreground pair and `B` swaps to it turned inside out, so both
the dark and the light background stay in the theme's palette rather than
falling back to flat black and white.

Light and dark follow the theme's own `mode`, so switching to a light Omarchy
theme switches the timer with it, live and without a restart. `B` overrides
that until the next theme change, which takes back over — picking a theme is
a fresh instruction, not something an old keypress should outrank.

Text uses iA Writer Mono S when it's installed (`ttf-ia-writer`), which is
what omacalc and omawrite use. Without it the timer falls back to the default
monospace face.

The window is painted opaque and keeps Omarchy's `default-opacity` tag, so
transparency is the compositor's job and Super+Alt+Backspace toggles this
window along with everything else.

## Differences from the Electron version

- Countdown runs against a wall-clock deadline, so a slow or blocked tick
  can't make the timer drift.
- Background choice and last duration persist across restarts
  (`~/.config/omarchy/omatimer.conf`), which was an open TODO in papertimer.
- The play, reset and contrast marks are drawn with QPainter rather than
  typed as glyphs, which several monospace faces are missing.
- A hairline under the number fills as the timer runs.
- Font size scales with the window instead of via CSS media queries. One
  unit derived from both width and height drives every size, so the layout
  holds its proportions in a short-wide or tall-narrow window.
- Sounds come from `/usr/share/sounds/freedesktop/stereo/`, so no bundled
  audio files. They're handed to `pw-play` (falling back to `paplay`) rather
  than Qt Multimedia, which would load FFmpeg, the VA-API video driver, GTK3
  and Qt Quick just to play a one-second chime — about 24 MB of PSS and a
  pile of VDPAU warnings on every launch.

## Measured against the Electron build

Both apps open, same machine (7.8 GB RAM), idle timer unless noted.

| | Electron papertimer | omatimer |
|---|---|---|
| Processes | 7 | 1 |
| Threads | 74 | 12 |
| RAM (PSS) | 242 MB | 41 MB |
| RAM (RSS) | 583 MB | 76 MB |
| CPU, idle | 1.30% | 0.05% |
| CPU, timer running | 0.83% | 0.43% |
| Install size | 446 MB | 60 KB binary |
| Files on disk | 10,972 | 6 |
| Cold start to window | 0.84 s | 0.38 s |

PSS is the fair memory number — it splits shared libraries between the
processes using them. RSS double-counts those, which is why Electron's
seven processes inflate it so much.
