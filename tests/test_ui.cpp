#include <cassert>
#include <iostream>
#include "aswell/ui/dom.hpp"
#include "aswell/ui/layout.hpp"
#include "aswell/ui/render.hpp"
#include "aswell/ui/color.hpp"
#include "aswell/ui/prompt.hpp"
#include "aswell/ui/animation.hpp"
#include "aswell/shell/environment.hpp"

using namespace aswell;

void test_dom_parsing() {
    std::string html = "<prompt class=\"main\"><user>alice</user><text>@</text><hostname>box</hostname></prompt>";
    auto dom = DOMParser::parse(html);
    assert(dom != nullptr);
    assert(dom->tag == "prompt");
    assert(dom->has_class("main"));
    assert(dom->children.size() == 3);
    assert(dom->children[0]->tag == "user");
    assert(dom->children[0]->text_content == "alice");
    std::cout << "[PASS] test_dom_parsing\n";
}

void test_color_and_visual_width() {
    Color c = Color::from_hex("#ff5555");
    assert(!c.is_none);
    assert(c.r == 255 && c.g == 85 && c.b == 85);

    std::string ansi_colored = "\033[31mhello\033[0m world";
    assert(str_util::visual_width(ansi_colored) == 11);
    assert(str_util::strip_ansi(ansi_colored) == "hello world");

    std::cout << "[PASS] test_color_and_visual_width\n";
}

void test_prompt_render() {
    Environment env;
    env.set_var("USER", "testuser");
    env.set_var("PWD", "/home/testuser");
    PromptEngine pe(env);
    auto ctx = pe.gather_context(0, 0, false);
    RenderResult res = pe.render(ctx, 0);
    assert(!res.ansi_output.empty());
    std::cout << "[PASS] test_prompt_render\n";
}

void test_animation_engine() {
    AnimationConfig anim;
    anim.type = AnimationType::SCRAMBLE;
    anim.duration_ms = 40;

    std::string base = "username";
    std::string s1 = AnimationEngine::evaluate_text(anim, base, 0);
    std::string s2 = AnimationEngine::evaluate_text(anim, base, 40);
    assert(s1.size() == base.size());
    assert(s2.size() == base.size());

    Color bg1 = AnimationEngine::evaluate_wave_bg(0, 0, 10);
    Color bg2 = AnimationEngine::evaluate_wave_bg(500, 0, 10);
    assert(!bg1.is_none && !bg2.is_none);

    std::cout << "[PASS] test_animation_engine\n";
}

void test_reactive_variable_expansion() {
    Environment env;
    env.set_var("CLUSTER", "production-us-east");
    env.set_var("CPU_LOAD", "1.42");
    PromptEngine pe(env);
    pe.set_template_html("<prompt><badge class=\"k8s\">☸ $CLUSTER</badge><badge class=\"cpu\">⚡ ${CPU_LOAD}</badge></prompt>");
    pe.set_theme_css(".k8s { color: #50fa7b; } .cpu { color: #ff5555; }");
    auto ctx = pe.gather_context(0, 0, false);
    RenderResult res = pe.render(ctx, 0);
    assert(res.ansi_output.find("production-us-east") != std::string::npos);
    assert(res.ansi_output.find("1.42") != std::string::npos);
    std::cout << "[PASS] test_reactive_variable_expansion\n";
}

void test_conditional_directives() {
    Environment env;
    env.set_var("STATUS", "0");
    env.set_var("CUSTOM_FLAG", "");
    PromptEngine pe(env);
    pe.set_template_html(
        "<prompt>"
        "<badge class=\"err\" show-if=\"$STATUS != 0\">✘ $STATUS</badge>"
        "<badge class=\"ok\" show-if=\"$STATUS == 0\">✓ OK</badge>"
        "<badge class=\"flag\" show-if=\"$CUSTOM_FLAG != ''\">FLAG: $CUSTOM_FLAG</badge>"
        "</prompt>"
    );
    auto ctx = pe.gather_context(0, 0, false);
    RenderResult res = pe.render(ctx, 0);
    assert(res.ansi_output.find("✓ OK") != std::string::npos);
    assert(res.ansi_output.find("✘") == std::string::npos);
    assert(res.ansi_output.find("FLAG:") == std::string::npos);

    // Now test with non-zero exit status and CUSTOM_FLAG set
    env.last_exit_status = 127;
    env.set_var("CUSTOM_FLAG", "ACTIVE");
    auto ctx2 = pe.gather_context(0, 0, false);
    RenderResult res2 = pe.render(ctx2, 0);
    assert(res2.ansi_output.find("✘ 127") != std::string::npos);
    assert(res2.ansi_output.find("✓ OK") == std::string::npos);
    assert(res2.ansi_output.find("FLAG: ACTIVE") != std::string::npos);

    std::cout << "[PASS] test_conditional_directives\n";
}

void test_rprompt_and_statusbar() {
    Environment env;
    env.set_var("SYS_LOAD", "0.24");
    PromptEngine pe(env);
    pe.set_template_html(
        "<prompt>"
        "<statusbar><text>DOCK</text></statusbar>"
        "<segment><text>PROMPT></text></segment>"
        "<rprompt><text>RIGHT</text></rprompt>"
        "</prompt>"
    );
    auto ctx = pe.gather_context(0, 0, false);
    RenderResult res = pe.render(ctx, 0);
    assert(!res.statusbar_ansi.empty());
    assert(res.statusbar_ansi.find("DOCK") != std::string::npos);
    assert(!res.rprompt_ansi.empty());
    assert(res.rprompt_ansi.find("RIGHT") != std::string::npos);
    assert(res.rprompt_width > 0);
    assert(res.ansi_output.find("PROMPT>") != std::string::npos);
    std::cout << "[PASS] test_rprompt_and_statusbar\n";
}

int main() {
    std::cout << "--- Running UI & Layout Tests ---\n";
    test_dom_parsing();
    test_color_and_visual_width();
    test_prompt_render();
    test_animation_engine();
    test_reactive_variable_expansion();
    test_conditional_directives();
    test_rprompt_and_statusbar();
    std::cout << "All UI Tests Passed!\n";
    return 0;
}
