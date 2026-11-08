/*
 * Minimal stand-in for AOSP's <cutils/multiuser.h>, used only by the NDK
 * build (jni/Android.mk). AOSP builds use the real libcutils header.
 */
#ifndef SUPERUSER_COMPAT_CUTILS_MULTIUSER_H
#define SUPERUSER_COMPAT_CUTILS_MULTIUSER_H

#include <sys/types.h>

#define MULTIUSER_APP_PER_USER_RANGE 100000

typedef uid_t userid_t;
typedef uid_t appid_t;

static inline userid_t multiuser_get_user_id(uid_t uid) {
    return uid / MULTIUSER_APP_PER_USER_RANGE;
}

static inline appid_t multiuser_get_app_id(uid_t uid) {
    return uid % MULTIUSER_APP_PER_USER_RANGE;
}

static inline uid_t multiuser_get_uid(userid_t user_id, appid_t app_id) {
    return user_id * MULTIUSER_APP_PER_USER_RANGE + (app_id % MULTIUSER_APP_PER_USER_RANGE);
}

#endif
