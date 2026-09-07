#include <cassert>
#include <iostream>
#include "aswell/editor/history.hpp"

using namespace aswell;

void test_history_basic() {
    History hist("/tmp/test_aswell_hist.txt");
    hist.clear();

    hist.add("echo alpha");
    hist.add("git status");
    hist.add("gcc -o main main.c -Wall");

    assert(hist.size() == 3);
    assert(hist.get(0) == "echo alpha");
    assert(hist.get(1) == "git status");
    assert(hist.get(2) == "gcc -o main main.c -Wall");

    hist.pop_last();
    assert(hist.size() == 2);
    assert(hist.get(1) == "git status");

    hist.clear();
    assert(hist.size() == 0);

    std::cout << "[PASS] test_history_basic\n";
}

void test_history_expansion() {
    History hist("/tmp/test_aswell_hist_exp.txt");
    hist.clear();

    hist.add("echo first second third");
    hist.add("git commit -m \"my commit message\"");

    std::string err;

    // 1. !! (previous command)
    auto r1 = hist.expand_history("sudo !!", err);
    assert(r1.has_value());
    assert(*r1 == "sudo git commit -m \"my commit message\"");

    // 2. !$ (last word of previous command)
    auto r2 = hist.expand_history("echo !$", err);
    assert(r2.has_value());
    assert(*r2 == "echo \"my commit message\"");

    // 3. !^ (first arg of previous command)
    auto r3 = hist.expand_history("echo !^", err);
    assert(r3.has_value());
    assert(*r3 == "echo commit");

    // 4. !* (all args of previous command)
    auto r4 = hist.expand_history("echo !*", err);
    assert(r4.has_value());
    assert(*r4 == "echo commit -m \"my commit message\"");

    // 5. !-2 (2 commands back)
    auto r5 = hist.expand_history("!-2", err);
    assert(r5.has_value());
    assert(*r5 == "echo first second third");

    // 6. !1 (first entry in history)
    auto r6 = hist.expand_history("!1", err);
    assert(r6.has_value());
    assert(*r6 == "echo first second third");

    // 7. !prefix
    auto r7 = hist.expand_history("!echo", err);
    assert(r7.has_value());
    assert(*r7 == "echo first second third");

    // 8. !?substring
    auto r8 = hist.expand_history("!?commit?", err);
    assert(r8.has_value());
    assert(*r8 == "git commit -m \"my commit message\"");

    // 9. Single quotes protect from expansion
    auto r9 = hist.expand_history("echo '!! and !$'", err);
    assert(r9.has_value());
    assert(*r9 == "echo '!! and !$'");

    // 10. Escaped exclamation mark
    auto r10 = hist.expand_history("echo \\!not_hist", err);
    assert(r10.has_value());
    assert(*r10 == "echo !not_hist");

    // 11. Literal exclamation followed by whitespace or =
    auto r11 = hist.expand_history("if ! cmd; then [ $x != 0 ]; fi", err);
    assert(r11.has_value());
    assert(*r11 == "if ! cmd; then [ $x != 0 ]; fi");

    // 12. Non-existent event
    auto r12 = hist.expand_history("!nonexistent", err);
    assert(!r12.has_value());
    assert(err.find("event not found") != std::string::npos);

    hist.clear();
    std::cout << "[PASS] test_history_expansion\n";
}

int main() {
    std::cout << "--- Running History Tests ---\n";
    test_history_basic();
    test_history_expansion();
    std::cout << "All History Tests Passed!\n";
    return 0;
}
