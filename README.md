# omatimer

A native Qt6 rewrite of papertimer. One binary, no runtime, no browser.

## Build

Needs `qt6-base` and `qt6-multimedia` (already installed on this machine).

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

Durations accept plain seconds (`90`) or units (`1h 30m 10s`, `1.5m`).

| Key | Action |
|-----|--------|
| Enter / Space | start or stop |
| R | reset to the saved duration |
| B | cycle background (dark / light / glass) |
| `[` / `]` (or `-` / `+`) | less / more transparent, in 10% steps |
| `\` | toggle glass on/off |
| Ctrl+M | mute |
| Esc | quit |

Shortcuts are case-insensitive and work while the duration box has focus;
`h`, `m`, `s`, digits and `.` still type normally, which is why mute moved to
Ctrl+M (a plain `m` is part of `25m`).

Only the third background ("glass") is see-through; dark and light are solid
panels. Reaching for `[` / `]` switches to glass, and `\` flips between glass
and whichever solid background you were on. The level persists across restarts.

Transparency needs a compositor that blends window alpha (Hyprland does).
Omarchy dims every window to `0.985/0.96` by default, which would multiply
against the app's own alpha, so this rule in `~/.config/hypr/hyprland.lua`
hands omatimer control of its own transparency:

```lua
o.window("omatimer", { tag = "-default-opacity", opacity = "1 1" })
```

## Differences from the Electron version

- Countdown runs against a wall-clock deadline, so a slow or blocked tick
  can't make the timer drift.
- Background choice, transparency, and last duration persist across restarts
  (`~/.config/omarchy/omatimer.conf`), which was an open TODO in papertimer.
- Font size scales with the window instead of via CSS media queries.
- Sounds come from `/usr/share/sounds/freedesktop/stereo/`, so no bundled
  audio files.

## Measured against the Electron build

Both apps open, same machine (7.8 GB RAM), idle timer unless noted.

| | Electron papertimer | omatimer |
|---|---|---|
| Processes | 7 | 1 |
| Threads | 74 | 12 |
| RAM (PSS) | 242 MB | 52 MB |
| RAM (RSS) | 583 MB | 102 MB |
| CPU, idle | 1.30% | 0.05% |
| CPU, timer running | 0.83% | 0.43% |
| Install size | 446 MB | 60 KB binary |
| Files on disk | 10,972 | 6 |
| Cold start to window | 0.84 s | 0.38 s |

PSS is the fair memory number — it splits shared libraries between the
processes using them. RSS double-counts those, which is why Electron's
seven processes inflate it so much.
