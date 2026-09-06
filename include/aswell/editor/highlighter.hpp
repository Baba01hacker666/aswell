#pragma once

#include "aswell/common.hpp"
#include "aswell/shell/environment.hpp"

namespace aswell {

class SyntaxHighlighter {
public:
    explicit SyntaxHighlighter(Environment& env);

    std::string highlight(const std::string& line) const;

private:
    Environment& env_;
};

} // namespace aswell
