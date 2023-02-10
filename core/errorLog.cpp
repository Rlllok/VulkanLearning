#include "errorLog.h"

namespace error
{

void log(std::string errorText)
{
    throw std::runtime_error("ERROR: " + errorText);
}

}