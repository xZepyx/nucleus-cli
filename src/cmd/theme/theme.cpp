#include <iostream>
#include <string>
#include <cstdlib>
#include <prompts/prompts.hpp>
#include <ipc/ipc.hpp>

namespace theme {

void change(std::string name) {
    std::string themeDir = "$HOME/.config/nucleus-shell/colorschemes"; // themes path
    std::string target = "$HOME/.config/nucleus-shell/config/colors.json"; // File to edit in order to load colors

    if (name == "") { // we are already evaluating name as "" in main if it's not passed by the user so "" will work as a fallback
        prompt::Fail("Usage: nucleus theme switch <theme-name | auto>");
        return;
    }
    else if (name == "auto" || name == "autogen") {
        prompt::Stage("Generating Theme");
        ipc::call("global", "regenColors", ""); // regen colors using the shell's exposed ipc (matugen)
    }
    else {
        std::string source = themeDir + "/" + name + ".json";
        int pathCheckResult = std::system(("ls " + source).c_str());
        std::string tmp = "mktemp";
        switch (pathCheckResult) {
            case 0:
                std::system((tmp + " >/dev/null 2>&1").c_str()); // send all logs to /dev/null to have a clean logging for the user.
                std::system(("cat " + source + " > " + tmp + " 2>/dev/null").c_str());
                std::system(("mv " + tmp + " " + target + " >/dev/null 2>&1").c_str());
                prompt::Success("Switched Theme:", name);
                break;
            // for both exit codes 1 and 2 log theme no found as ls might exit with code 1/2 on invalid path;
            case 1:
                prompt::Fail("Theme not found"); 
                break;
            case 2:
                prompt::Fail("Theme not found");
                break;
            case 512: // it might also return 512
                prompt::Fail("Theme not found");
                break;
            default:
                prompt::Fail("Failed, Exit Code:", pathCheckResult);
                break;
        }
    }

}

}