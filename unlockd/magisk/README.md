# unlockd Magisk module

This module installs the `unlockd` AArch64 daemon under `system/bin` and
starts it from `service.sh` after Android reports boot completion.

Build the native daemon first, then run `build-module.bat` from this directory
or from the `unlockd` directory. Install the generated ZIP from Magisk.

The daemon listens on TCP and UDP port `8765`. Its log is written to
`/data/adb/unlockd.log`.
