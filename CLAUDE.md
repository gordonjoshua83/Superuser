# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

An open-source Android root manager (fork of Koushik Dutta's Superuser, maintained by phhusson). It has two halves that talk to each other at runtime:

- **`su` binary** (C, `Superuser/jni/su/`): a statically linked executable that doubles as a daemon.
- **Manager APK** (Java, `Superuser/src/com/koushikdutta/superuser/`): prompts the user to allow or deny requests, and holds the policy database and logs.

Everything lives under the nested `Superuser/` directory (not the repo root). The root only has `README.md`, `LICENSE`, `Android.mk` (AOSP build) and `SELinux/Policy.md`.

## Build commands

Run these from `Superuser/Superuser/`, unless the AOSP build is noted:

- `ndk-build` builds `su` for x86, armeabi and mips (see `jni/Application.mk`). Output goes to `libs/<abi>/su`. The README says NDK r10e is known to work.
- `./gradlew assembleDebug` builds the APK. It needs the Android SDK with API 22 and build-tools 22.0.1, and uses Gradle 2.5 with Android plugin 1.2.3. The APK packs `libs/` as `jniLibs`, so run `ndk-build` first.
- `./build-zip.sh` packs `libs/{x86,mips,armeabi}` plus `assets/update-binary` and `assets/install-recovery.sh` into `update-su.zip`, a recovery-flashable installer.
- Release signing: setting `KEYSTORE` makes Gradle prompt for the store and key passwords on the console (alias `su`).
- There are no tests and no lint configuration in the repo.
- The build files target a 2015-era toolchain (Gradle 2.5, Android plugin 1.2.3, API 22, NDK r10e). Current Android docs for AGP, Gradle and the NDK describe newer, incompatible setups, so don't "modernize" the build files as a side effect of other work.
- `assets/update-binary` copies `$PLATFORM/placeholder` into `/system/bin/app_process32`, so the installer expects a `placeholder` binary next to `su` in `libs/<abi>/`. Its source is not in `jni/`, and `jni/Android.mk` does not build it.

**`Widgets` dependency:** `build.gradle` depends on `:Widgets`. It is a git submodule (`phhusson/Widgets`) that must sit as a sibling of `Superuser/`, at the repo root next to the `Superuser/` project dir. Run `git submodule update --init`, or clone it by hand as the README says.

**Package name:** `Superuser/packageName` (currently `me.phh.superuser`) is read by two things: Gradle uses it as `applicationId`, and `jni/Android.mk` bakes it into the binary as `-DREQUESTOR="..."`. The `su` binary hardcodes the APK package so it can start the APK's activities. If you change one without rebuilding the other, the daemon will start the wrong package. The Java package and class names stay `com.koushikdutta.superuser` regardless (`JAVA_PACKAGE_NAME` in `su.h` says not to change it).

**AOSP build:** the root `Android.mk` builds `su` inside an AOSP tree (it expects `SUPERUSER_PACKAGE` and optionally `SUPERUSER_PACKAGE_PREFIX` and `SUPERUSER_EMBEDDED`). It points at `Superuser/jni/...` paths. `Superuser/jni/Android.mk` is the NDK/AOSP-subtree variant. Keep the `LOCAL_SRC_FILES` lists in both in sync when adding C files.

## Architecture

**Daemon model (`jni/su/`):** `su.c` `main()` dispatches. `su --daemon` calls `run_daemon()` in `daemon.c`, and anything else calls `su_main()`, which connects to the daemon over a unix socket (`connect_daemon`). The daemon forks a child per request (`run_daemon_child`), which hands the client's pty and stdio to a root shell. The daemon is started at boot by `install-recovery.sh` or the AOSP `init.superuser.rc`. `pts.c` handles pty plumbing.

**Request flow:**
1. A client runs `su`, which connects to the daemon.
2. `db.c` checks `database_check()` against the manager's sqlite DB (`REQUESTOR/databases/su.sqlite`). The result is `policy_t` INTERACTIVE, DENY or ALLOW.
3. If the result is INTERACTIVE, `activity.c` launches the APK's `RequestActivity` by shelling out to `am` through `app_process`, and the user's answer comes back over the socket.
4. After the command runs, `activity.c` sends the result to the APK's `SuReceiver` as a broadcast for logging and notifications.

`su.c`, `daemon.c`, `activity.c` and `db.c` all communicate via `struct su_context`. The structs and constants in `su.h` (paths under `/data/data/<REQUESTOR>` and `/dev/<REQUESTOR>.daemon`, multiuser modes, `PROTO_VERSION`) define the contract with the Java side.

**Static-binary constraints:** `su` is built `-static` against libc, libcutils and libselinux. It cannot use `liblog`, so `LOGE`/`LOGD`/`LOGW` go through `exec_log*` (the `/system/bin/log` binary). Do not add dependencies on dynamic Android libraries. The C is `-std=gnu11`.

**Vendored libraries in `jni/`:** `sqlite3/` is the SQLite amalgamation, built with `SQLITE_OMIT_LOAD_EXTENSION` and linked into `su`. `libselinux/` and `libsepol/` are trimmed copies of the AOSP versions, wired in through their own `Android.mk`. Treat all three as third-party and avoid edits unless required.

**Other su features:** `binds.c` handles `su --bind`. `hacks.c` holds per-device workarounds, and `hacks_init()` and `hacks_update_context()` hook into the main flow.

**Multiuser:** Android 4.2+ multiuser behaviour is selected by `multiuser_mode_t` (owner-only, owner-managed, user, none) and is read from a `multiuser_mode` file in the app's files dir. The path logic is in `su.h`, `db.c` and `utils.c`.

**Manager app (`src/com/koushikdutta/superuser/`):** `RequestActivity` and `MultitaskSuRequestActivity` are the allow/deny prompts. `NotifyActivity` and `SuReceiver` handle result notifications. `db/` holds `SuDatabaseHelper`, `UidPolicy` and `LogEntry`. Policy and log are shown by `PolicyFragment` and `LogFragment`, under `MainActivity`. `su.sqlite` holds only the `uid_policy` table, which `SuDatabaseHelper` creates and migrates (schema version 6) and which `db.c` reads with a hardcoded `select policy, until, command from uid_policy where uid=...`. If you rename or restructure those columns, change both sides together. Logs and settings live in a separate DB (`SuperuserDatabaseHelper`) so that `su` only needs a read-only handle on `su.sqlite`.

**SELinux:** `SELinux/Policy.md` is the design doc for the `su` and `su_sensitive` domains. The policy rules themselves are not in this repo, only the intent.

## Notes

- Install methods (placeholder `app_process32` swap, boot.img patching, AOSP patch) are described in `README.md`. The placeholder method is obsolete on Android M and later.
- The README asks that security flaws be reported privately to the maintainer, not through public issues.
