#pragma once
#include <string>

namespace installer {
void installPackage(const std::string &helper, const std::string &pkg);
int getAurHelper();
void installShell();
} // namespace installer
