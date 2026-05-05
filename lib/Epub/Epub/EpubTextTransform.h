#pragma once

#include <string>

namespace EpubTextTransform {

std::string simplifiedToTraditional(const char* text, int len);
std::string simplifiedToTraditional(const std::string& text);

}  // namespace EpubTextTransform
