# omatimer

A native Qt6 rewrite of papertimer. One binary, no runtime, no browser.

## Install

```sh
git clone https://github.com/isjake/omatimer.git
cd omatimer && ./install.sh
```

It installs any missing build tools (`qt6-base`, `base-devel`; asks for your
password only then), builds the app, and puts it in your app launcher. Nothing
goes outside your home folder. `./install.sh --remove` uninstalls;
`./install.sh --link` links to the build in this folder instead of copying it,
so `make` updates the installed app (handy while working on it).

Sounds play with `pw-play` from `pipewire` (or `paplay`). The timer makes its
own default sounds, and carries a copy of the freedesktop sound theme in
`sounds/` for the rest, so nothing else needs installing.

## Build by hand

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
| Ctrl+M | mute (also the speaker button, next to reset) |
| ? / F1 | show or hide the shortcuts and the volume slider (also the ? button, bottom right) |

Shortcuts are case-insensitive and work while the duration box has focus;
`h`, `m`, `s`, digits and `.` still type normally, which is why mute moved to
Ctrl+M (a plain `m` is part of `25m`).

## Volume

Sounds play at 20% of the system volume by default. Mute (Ctrl+M or the
speaker button) is remembered too, so a new timer opens muted if the last one
was. The slider in the help
window (`?`) changes that; it's saved straight away and shared by every open
timer. It sets the sound's own volume, so it still follows the system volume.

## Sounds

The help window (`?`) also picks the **button sound** and the **done sound**.
The defaults are built in (*chime* when done, *click* for buttons, plus *bell*),
made by the timer itself, so they work everywhere. The rest are the freedesktop
sound theme, grouped as good-for-timers and other system sounds, or None. The
system's copy (`/usr/share/sounds/freedesktop/stereo/`) is used when installed,
otherwise the copy in `sounds/freedesktop/`. **That copy is not MIT**: the
files keep their own GPL / LGPL / Creative Commons licenses, listed in
`sounds/freedesktop/CREDITS`. Picking one plays a preview.
Both are saved as soon as they change.

The **font** always matches the system: fontconfig's `monospace`, the font
`omarchy font set` changes. Open timers switch to a new system font straight
away, no restart needed.

## Colors

Colors come from the current Omarchy theme
(`~/.local/state/omarchy/current/theme/colors.toml`), the same file omacalc
and omawrite read, and re-tint live when you switch themes. The theme gives
one background/foreground pair; a light theme uses it turned inside out, so the
light background stays in the theme's palette rather than flat white.

Light and dark follow the theme's own `mode`, so switching to a light Omarchy
theme switches the timer with it, live and without a restart. There's no manual
light/dark toggle.

The window is painted opaque and keeps Omarchy's `default-opacity` tag, so
transparency is the compositor's job and Super+Alt+Backspace toggles this
window along with everything else.

## Differences from the Electron version

- Countdown runs against a wall-clock deadline, so a slow or blocked tick
  can't make the timer drift.
- Last duration persists across restarts
  (`~/.config/omarchy/omatimer.conf`), which was an open TODO in papertimer.
- The play, reset, mute and help marks are drawn with QPainter rather than
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

## License

MIT. See [LICENSE](LICENSE). The sounds in `sounds/freedesktop/` are not
covered by it; see that folder's README.
