## What's new in 2.3.2: Setup fix

- **Setup no longer fails with "manifest 'build.ninja' still dirty after 100 tries".** Installing soon after a release, on a PC whose clock is behind the release server's time zone, the unpacked files were dated in the future, and the build tool kept regenerating its build files without end. Setup now dates the files it unpacks with your clock, and the release's files are dated a day back as well.

If an install stopped with that message, run this Setup again over the same folder: it unpacks the source anew and carries on. Everything from 2.3.1 is in this release.
