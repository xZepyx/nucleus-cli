#include <iostream>
#include <cstdlib>
#include <string>
#include <ipc/ipc.hpp>

namespace ipc {

void call(std::string target, std::string function, const std::string& args) {
    std::string cmd = "quickshell -c nucleus-shell ipc call " + target + " " + function + " " + args; // add spaces to seperate args
    std::system(cmd.c_str());
}

void show() {
    std::system("quickshell -c nucleus-shell ipc show");
}

}

