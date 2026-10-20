# Spirit FM for modern Android

A port of **Spirit2** – Michael A. Reid's over-the-air FM radio app – to current Android (tested on Android 16), with a new tuner backend for the **MediaTek FM chips** used in recent Motorola phones.

Mike wrote Spirit1, Spirit2 and Spirit3 and supported hundreds of phones, most of them by talking to the FM chip directly. He released Spirit2 Free as open source in 2014. This fork keeps his code and history and carries it forward to hardware he never got to see. It is dedicated to his memory.

## Status

| Device | SoC / FM chip | Status |
|---|---|---|
| moto g 2026 (`utah`) | MediaTek MT6835 / MT6631 | Working: tune, seek, pause, power, speaker audio, RDS station name and radio text |

Other MediaTek phones that expose `/dev/fm` (connsys FM, MT6631/MT6635) will probably work. Spirit2's original Qualcomm, Broadcom and Samsung Silicon Labs backends are still built, but they have not been tested on modern Android.

## Requirements

- **Root** (KernelSU or Magisk). Grant Spirit FM root in your root manager.
- **Wired headphones or a headphone cable plugged in.** The cable is the FM antenna. Audio plays on the loudspeaker by default, so the cable doesn't have to be real headphones.
- No system module or reboot is needed.

## How it works on MediaTek

- **Tuner:** `jni/tnr/tnr_mtk.c` drives `/dev/fm` with MediaTek's FM ioctls from Spirit's root daemon (`s2d`, packaged as `libs2d.so`). Seek uses the chip's own soft-mute channel check, the same way MediaTek's HAL does. RDS records are read from the driver; their layout was measured on the MT6631 driver, which differs from MediaTek's published header.
- **Audio:** Android only lets privileged apps or root capture the FM tuner (`AudioSource.RADIO_TUNER`). Spirit therefore starts a small root helper (`rootaudio.java`, run with `app_process` from the APK). The helper copies tuner audio to an `AudioTrack` and routes media to the loudspeaker.
- **Android 14+ fixes:** dynamic receiver export flags, immutable PendingIntents, a typed foreground service, non-sticky status broadcasts, going foreground before talking to the daemon, and a daemon run flag kept in the app's own data directory instead of `/dev`.

## Building

```
./gradlew :app:assembleDebug
```

You need the Android SDK (compileSdk 36) and NDK 29.0.14206865. Set `sdk.dir` in `local.properties`. Builds are arm64-v8a only.

## History and credits

- **Michael A. Reid** – Spirit1, Spirit2, Spirit3, and the Spirit2 Free source in this repo (2011–2015). His original notes are in `README_OPENSOURCE_BUILDING.txt`.
- **Saylance** – the reconstructed `2015_12_22` update ([Saylance/spirit2_radio](https://github.com/Saylance/spirit2_radio)), included here with its original commits.
- **JeremyDoege89** – Gradle/NDK build, the Android 16 port, and the MediaTek tuner, RDS and audio backends.

## License

GNU Affero General Public License v3, as released by Michael A. Reid. See `COPYRIGHT_MICHAEL_REID_GPLv3AFFERO.txt`. If you distribute a modified version, you must make its source available under the same license.
