#include "aswell/shell/builtins.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace aswell {

int Builtins::builtin_test(const std::vector<std::string>& raw_args, Environment& /*env*/) {
    std::vector<std::string> args = raw_args;
    if (!args.empty() && args[0] == "[") {
        if (args.back() == "]") {
            args.pop_back();
        } else {
            std::cerr << "aswell: [: missing ']'\n";
            return 2;
        }
    }

    if (args.size() <= 1) return 1; // Empty test is false

    bool invert = false;
    size_t idx = 1;
    if (args[idx] == "!") {
        invert = true;
        idx++;
    }

    size_t remaining = args.size() - idx;
    bool result = false;

    if (remaining == 1) {
        result = !args[idx].empty();
    } else if (remaining == 2) {
        const std::string& op = args[idx];
        const std::string& target = args[idx + 1];
        struct stat st;

        if (op == "-f") result = (stat(target.c_str(), &st) == 0 && S_ISREG(st.st_mode));
        else if (op == "-d") result = (stat(target.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
        else if (op == "-e") result = (stat(target.c_str(), &st) == 0);
        else if (op == "-r") result = (access(target.c_str(), R_OK) == 0);
        else if (op == "-w") result = (access(target.c_str(), W_OK) == 0);
        else if (op == "-x") result = (access(target.c_str(), X_OK) == 0);
        else if (op == "-s") result = (stat(target.c_str(), &st) == 0 && st.st_size > 0);
        else if (op == "-z") result = target.empty();
        else if (op == "-n") result = !target.empty();
        else if (op == "-L" || op == "-h") result = (lstat(target.c_str(), &st) == 0 && S_ISLNK(st.st_mode));
        else if (op == "-t") {
            int fd = 0;
            try { fd = std::stoi(target); } catch (...) { fd = -1; }
            result = (fd >= 0 && isatty(fd));
        }
    } else if (remaining == 3) {
        const std::string& left = args[idx];
        const std::string& op = args[idx + 1];
        const std::string& right = args[idx + 2];

        if (op == "=" || op == "==") result = (left == right);
        else if (op == "!=") result = (left != right);
        else {
            long long l = 0, r = 0;
            try { l = std::stoll(left); } catch (...) {}
            try { r = std::stoll(right); } catch (...) {}

            if (op == "-eq") result = (l == r);
            else if (op == "-ne") result = (l != r);
            else if (op == "-lt") result = (l < r);
            else if (op == "-le") result = (l <= r);
            else if (op == "-gt") result = (l > r);
            else if (op == "-ge") result = (l >= r);
        }
    }

    if (invert) result = !result;
    return result ? 0 : 1;
}

} // namespace aswell
