# Bundled freedesktop sounds

A copy of the freedesktop sound theme (`sound-theme-freedesktop` 0.8,
https://freedesktop.org/wiki/Specifications/sound-theme-spec), so the timer
has its full sound list on systems that don't install it, like a fresh Omarchy.
The speaker-test tones (`audio-channel-*`, `audio-test-signal`) are left out;
the app never lists them.

These files are **not** under omatimer's MIT license. Each keeps its own
license (GPL-2.0, LGPL-2.0, CC-BY-SA-3.0, CC-BY-3.0 or CC-BY-4.0); see
`CREDITS` for which file is which and who made it.

The app works without this folder: it has its own built-in sounds, and uses
the system's copy of these when it's installed. Deleting the folder
just removes them from the list on machines without the system copy.
