#pragma once

#ifdef VK_USE_PLATFORM_ANDROID_KHR
#include <android/log.h>

#define TAG "VulkanLOG"
#define LOG_ERROR(...) ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))
#define LOG_INFO(...) ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define LOG_VL(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "VulkanVL", __VA_ARGS__))

#endif

#include <iostream>
#include <stdexcept>

namespace error
{


void log(std::string errorText);

}