#pragma once

#include <android/log.h>

#define UVP_TAG "UVPainter"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, UVP_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, UVP_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, UVP_TAG, __VA_ARGS__)

#ifdef NDEBUG
#define LOGD(...) ((void)0)
#else
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, UVP_TAG, __VA_ARGS__)
#endif
