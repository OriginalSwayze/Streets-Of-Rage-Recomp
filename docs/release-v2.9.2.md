## What's new in 2.9.2: Setup says why it stops

Fixed:

- **Setup stuck at 1% (#28).** When an install couldn't start (unpacking the collection, or copying a ROM, went wrong), Setup kept the error to itself and stayed on the progress page at 1%. It now stops and shows why.
- **Linux, through Wine or Proton:** this was where it stuck. Setup builds each game with Windows' own PowerShell and tar, which Wine doesn't have, so it can't install there yet. It now checks for both before it starts and says so, instead of hanging. Making it install under Wine is being worked on.

Not fixed yet:

- **Sprites flickering in 7A's robot factory when it's crowded (#21).**
- **A light black flicker with high frame rate (Streets of Rage 2),** **round 6's screen unlock glitch (#20, the other parts)** and **the background jitter at the start of a level (#17).**

Everything from 2.9.1 is in this release. If you have 2.9 or later, the collection offers this update when it opens.
