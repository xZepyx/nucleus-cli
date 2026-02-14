#pragma once 
#include <string>

namespace ipc {

void call(std::string target, std::string function, const std::string& args);
void show();

}