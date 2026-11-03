#pragma once

#include <string>

VCMI_LIB_NAMESPACE_BEGIN
class CGObjectInstance;
VCMI_LIB_NAMESPACE_END

namespace ObjectPreview
{
bool makeObjectPreview(const CGObjectInstance * object, std::string & text);
}
