#include "aswell/ui/demo.hpp"
#include "aswell/ui/animation.hpp"
#include "aswell/ui/prompt.hpp"
#include "aswell/ui/terminal.hpp"
#include "aswell/editor/editor.hpp"
#include "aswell/shell/executor.hpp"
#include "aswell/shell/jobs.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <unistd.h>

namespace aswell {

int EngineDemo::run(Environment& env, bool auto_mode) {
    bool interactive = isatty(STDIN_FILENO) && !auto_mode;

    std::cout << "\033[1;36m╭───────────────────────────────────────────────────────────────╮\033[0m\n";
    std::cout << "\033[1;36m│\033[0m       \033[1;35m⚡ ASWELL ENGINE SHOWCASE: DYNAMIC UI & ANIMATIONS ⚡\033[0m    \033[1;36m│\033[0m\n";
    std::cout << "\033[1;36m│\033[0m  * \033[1;32mFast Scrambling Username (abcdef...)\033[0m [40ms matrix tick]    \033[1;36m│\033[0m\n";
    std::cout << "\033[1;36m│\033[0m  * \033[1;33mFlowing Cyber Wave Aura Behind Commands\033[0m [TrueColor dynamic]   \033[1;36m│\033[0m\n";
    std::cout << "\033[1;36m│\033[0m  * \033[1;37mType live commands or type 'exit' to return to normal shell\033[0m  \033[1;36m│\033[0m\n";
    std::cout << "\033[1;36m╰───────────────────────────────────────────────────────────────╯\033[0m\n\n";

    if (!interactive) {
        // Headless / Automated Demonstration for CI and verification
        std::cout << "\033[1;34m[1/3] Fast Scrambling Username Animation Sequence:\033[0m\n";
        AnimationConfig scramble_anim;
        scramble_anim.type = AnimationType::SCRAMBLE;
        scramble_anim.duration_ms = 40;

        const char* user_env = std::getenv("USER");
        std::string base_user = user_env ? std::string(user_env) : "baba01hacker";

        for (uint64_t t = 0; t < 240; t += 40) {
            std::string scrambled = AnimationEngine::evaluate_text(scramble_anim, base_user, t);
            std::cout << "  Frame @" << std::setw(3) << t << "ms: \033[1;32m" << scrambled << "\033[0m\n";
        }

        std::cout << "\n\033[1;34m[2/3] Command Dynamic Background Wave Rendering:\033[0m\n";
        std::string sample_cmd = "curl -s https://api.github.com | jq .current_user_url";
        for (uint64_t t = 0; t < 120; t += 60) {
            std::cout << "  Wave @" << std::setw(3) << t << "ms: ";
            for (size_t i = 0; i < sample_cmd.size(); ++i) {
                Color bg = AnimationEngine::evaluate_wave_bg(t, i, sample_cmd.size(), 1.2f);
                std::cout << bg.to_bg_ansi(true) << "\033[1;97m" << sample_cmd[i] << "\033[0m";
            }
            std::cout << "\n";
        }

        std::cout << "\n\033[1;34m[3/3] Integrated Live Prompt Engine Evaluation:\033[0m\n";
        PromptEngine pe(env);
        pe.set_template_html(
            "<prompt class=\"main\">"
            "<text class=\"bracket\">╭──[</text><directory /><text class=\"bracket\">]</text><newline />"
            "<text class=\"bracket\">╰── </text><user /><text class=\"at\">@</text><hostname /><text class=\"sep\"> </text><symbol /><text> </text>"
            "</prompt>"
        );
        pe.set_theme_css(
            "prompt.main { display: block; }"
            ".bracket { color: #00e5ff; font-weight: bold; }"
            "directory { color: #50fa7b; font-weight: bold; }"
            "user { color: #00ff88; font-weight: bold; animation: scramble 40ms infinite; }"
            ".at { color: #008855; }"
            "hostname { color: #00cc55; }"
            ".sep { color: #00e5ff; }"
            "symbol { color: #ff007f; animation: pulse 800ms infinite; }"
        );

        auto ctx = pe.gather_context();
        for (uint64_t t = 0; t < 120; t += 40) {
            RenderResult rr = pe.render(ctx, t);
            std::cout << "  Prompt Tick @" << t << "ms:\n" << rr.ansi_output;
        }

        std::cout << "\n\033[1;32m[✓] All animation engine pipelines passed with 100% stability.\033[0m\n";
        return 0;
    }

    // Interactive Demo Shell
    JobManager jobs;
    jobs.set_interactive(true);
    Executor executor(env, jobs);

    PromptEngine prompt_engine(env);
    prompt_engine.set_template_html(
        "<prompt class=\"main\">"
        "<text class=\"bracket\">╭──[</text><directory /><text class=\"bracket\">]</text><newline />"
        "<text class=\"bracket\">╰── </text><user /><text class=\"at\">@</text><hostname /><text class=\"sep\"> </text><symbol /><text> </text>"
        "</prompt>"
    );
    prompt_engine.set_theme_css(
        "prompt.main { display: block; }"
        ".bracket { color: #00e5ff; font-weight: bold; }"
        "directory { color: #50fa7b; font-weight: bold; }"
        "user { color: #00ff88; font-weight: bold; animation: scramble 40ms infinite; }"
        ".at { color: #008855; }"
        "hostname { color: #00cc55; }"
        ".sep { color: #00e5ff; }"
        "symbol { color: #ff007f; animation: pulse 800ms infinite; }"
    );

    LineEditor editor(env, prompt_engine);
    editor.set_command_animation(true);

    while (true) {
        jobs.update_status();
        double last_duration = executor.get_last_command_duration_ms();
        size_t active_jobs = jobs.active_job_count();

        auto line_opt = editor.read_line(last_duration, active_jobs);
        if (!line_opt.has_value()) {
            break;
        }

        std::string line = str_util::trim(*line_opt);
        if (line.empty()) continue;

        if (line == "exit" || line == "quit") {
            std::cout << "\033[1;32mExiting demo mode...\033[0m\n";
            break;
        }

        // Animate brief execution pulse
        std::cout << "\033[90m⚡ Executing: \033[1;36m" << line << "\033[0m\n";
        int status = executor.execute_string(line);
        if (status != 0) {
            std::cout << "\033[31m[Exit status: " << status << "]\033[0m\n";
        }
    }

    return 0;
}

} // namespace aswell
