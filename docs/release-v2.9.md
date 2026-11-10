## What's new in 2.9: the collection updates itself

**Updates from the collection.** When a newer release is out, the collection says so at the top as it opens.

- **Update** (U, or Y on a pad) downloads the new installer, checks it against the release, and runs it. It rebuilds the games you have from the ROMs Setup kept, with nothing to pick again, and keeps your settings, saves and desktop icon.
- **Not now** (N, or X on a pad) hides the sign until the release after that one.
- The check asks GitHub for the collection's latest release and nothing else. To turn it off, set `check_updates=0` in `collection.ini`, next to the collection's program.

This release has to be installed by hand, as before. After that, the collection offers each new release itself.

Not fixed yet:

- **A light black flicker with high frame rate (Streets of Rage 2):** not seen in the test runs yet.
- **Round 6's screen unlock glitch (#20, the other parts)** and **the background jitter at the start of a level (#17).**

Everything from 2.8.3 is in this release.
