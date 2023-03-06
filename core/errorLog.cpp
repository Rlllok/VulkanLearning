#include "errorLog.h"

namespace error
{

void log(std::string errorText)
{
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    LOG_ERROR("%s", errorText.c_str());
#else
    throw std::runtime_error("ERROR: " + errorText);
#endif
}

}