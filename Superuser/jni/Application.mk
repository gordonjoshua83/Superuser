# armeabi and mips are no longer supported by the NDK (removed in r17).
APP_ABI := armeabi-v7a arm64-v8a x86 x86_64
# Static executable; the minimum API level the current NDK supports.
APP_PLATFORM := android-21
APP_PIE := true
