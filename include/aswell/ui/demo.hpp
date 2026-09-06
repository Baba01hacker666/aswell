#pragma once

#include "aswell/common.hpp"
#include "aswell/shell/environment.hpp"

namespace aswell {

class EngineDemo {
public:
    static int run(Environment& env, bool auto_mode = false);
};

} // namespace aswell
