# SMART BOX Android app (skeleton)

Minimal Gradle project — status / unlock / lock against the same REST API
the web dashboard uses (see `src/main/java/.../SmartBoxApi.kt`).

## Opening this project

The Gradle wrapper jar/scripts (`gradlew`, `gradlew.bat`,
`gradle/wrapper/gradle-wrapper.jar`) are not included — binary files don't
travel well through this delivery path. Two ways to get them:

1. **Easiest**: open this folder in Android Studio. It detects the missing
   wrapper and offers to regenerate it automatically.
2. **CLI**: with a local Gradle install, run `gradle wrapper --gradle-version 8.7`
   from this directory.

`gradle-wrapper.properties` (already included) pins the version so either
path produces a consistent build.

## What's implemented vs. TODO

Implemented: connect to a device by IP/hostname, live status, UNLOCK/LOCK,
"open full dashboard" (opens the device's own web UI in the system
browser for everything else).

`SmartBoxApi.kt` already has `users()`, `cards()`, `events()`,
`diagnostics()` calls ready — wiring them into dedicated screens (e.g. a
bottom nav with Users/Cards/Events tabs) is the natural next step once the
core loop is validated against real hardware. Push notifications (spec
section 64) need a server-side relay (FCM) the ESP32 doesn't have — see
the note in `SmartBoxApi.kt`.
