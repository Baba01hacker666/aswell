#include "aswell/common.hpp"
#include "aswell/shell/environment.hpp"
#include "aswell/ui/demo.hpp"

int main(int argc, char* argv[]) {
    aswell::Environment env;
    bool auto_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--auto") {
            auto_mode = true;
        }
    }
    return aswell::EngineDemo::run(env, auto_mode);
}
