/**
* @Author   Mathieu Chassara
*/

#include <iostream>

#include <Engine/Utils/Debug.hpp>
#include <Engine/Utils/Logger.hpp>

bool    Debug::engineAssert(bool expression, std::string message, const char* filename,
                      const char* function, unsigned int line)
{
        if (expression == false)
        {
            LOG_ERROR("Assertion failed: \"%s\" in file %s, at %s (line %d)", message.c_str(), filename, function, (int)line);
            return (false);
        }
    return (true);
}
