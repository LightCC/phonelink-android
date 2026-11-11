# Phonelink workspace

Source of truth is on Mirror in this repository. Use SSH for source changes.
Transfer a source snapshot to the target Omarchy PC for Qt/Arch builds, tests and installation.
Mirror currently runs Mint and has no Qt build toolchain.
Do not commit or push unless the user asks.

Use C++17, Qt Quick and qmake6. Keep ADB and scrcpy operations asynchronous and bounded.
Run bin/test after backend changes. Use fake devices for failure tests; never reset real ADB keys.
Preserve existing phone history, previous endpoints and user configuration.
Do not enable legacy adb tcpip 5555. Verify the selected phone serial before mirroring.
Closing the panel hides it; intentional scrcpy closure must not trigger crash recovery.
Follow the Omarchy app skill for theme, language, packaging and visual QA.
