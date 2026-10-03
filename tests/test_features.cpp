// Unit tests for the parallel-execution helpers and the curated alias library:
// the pure string logic (template substitution, job splitting) plus the library
// invariants that would otherwise only be visible by hand in an interactive shell.
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

#include "aswell/config/alias_library.hpp"
#include "aswell/config/config.hpp"
#include "aswell/shell/environment.hpp"
#include "aswell/shell/executor.hpp"
#include "aswell/shell/jobs.hpp"
#include "aswell/shell/parallel.hpp"

using namespace aswell;

namespace {

// Runs `script` and returns everything it wrote to stdout.
inline std::string capture_stdout(Executor& executor, const std::string& script) {
    std::ostringstream buffer;
    auto* saved = std::cout.rdbuf(buffer.rdbuf());
    executor.execute_script(script);
    std::cout.rdbuf(saved);
    return buffer.str();
}

// Asserts on the exact stdout of a script, because "the rest of the input still
// runs" is the property under test and only an end-to-end check proves it.
#define ASSERT_CAPTURED(executor_ref, script, expected)                                   \
    do {                                                                                  \
        const std::string got_ = capture_stdout(executor_ref, script);                     \
        if (got_ != (expected)) {                                                          \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "\n  script:   "       \
                      << (script) << "\n  expected: [" << (expected) << "]\n  got:      [" \
                      << got_ << "]\n";                                                    \
            assert(false);                                                                 \
        }                                                                                  \
    } while (0)

std::string read_all(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// A private config dir, so the alias file the library reads and writes cannot
// touch the developer's real ~/.config/aswell.
class ScopedConfigDir {
public:
    ScopedConfigDir() {
        char tmpl[] = "/tmp/aswell_features_test_XXXXXX";
        const char* made = mkdtemp(tmpl);
        assert(made != nullptr);
        path_ = made;
        ::setenv("ASWELL_CONFIG_DIR", path_.c_str(), 1);
    }
    ~ScopedConfigDir() {
        ::unlink(AliasLibrary::file_path().c_str());
        ::rmdir(path_.c_str());
        ::unsetenv("ASWELL_CONFIG_DIR");
    }
private:
    std::string path_;
};

} // namespace

// ---------------------------------------------------------------------------
// parallel: job template substitution, input splitting, concurrency default
// ---------------------------------------------------------------------------

static void test_apply_template() {
    // The item is quoted, so spaces stay a single argument.
    assert(parallel_apply_template("rm -v {}", "a b") == "rm -v 'a b'");
    // No placeholder: the item is appended as an extra argument, like xargs.
    assert(parallel_apply_template("gzip", "file.txt") == "gzip file.txt");
    // {.} drops only the final extension.
    assert(parallel_apply_template("convert {.} {.}.pdf", "report.tar.gz") ==
           "convert report.tar report.tar.pdf");
    // `\{}` is an escaped placeholder: it survives literally.
    assert(parallel_apply_template("echo \\{}", "{}") == "echo {}");
    // Shell syntax in an item is data, not code: it must stay inside quotes.
    std::string out = parallel_apply_template("echo {}", "a; rm -rf /");
    assert(out.size() > 6 && out[5] == '\'');
    std::cout << "[PASS] test_apply_template\n";
}

static void test_split_lines() {
    std::vector<std::string> got = parallel_split_lines("  one \n\ntwo\n\n  three  \n", '\n');
    assert(got.size() == 3);
    assert(got[0] == "one" && got[1] == "two" && got[2] == "three");
    assert(parallel_split_lines("", '\n').empty());
    assert(parallel_split_lines("\n \n\t\n", '\n').empty());
    std::vector<std::string> cols = parallel_split_lines("a, b ,,c", ',');
    assert(cols.size() == 3 && cols[2] == "c");
    std::cout << "[PASS] test_split_lines\n";
}

static void test_default_jobs() {
    assert(parallel_default_jobs(7) == 7);           // an explicit request wins
    assert(parallel_default_jobs(5000) == 5000);     // even an absurd one
    assert(parallel_default_jobs(0) >= 1);           // otherwise: one per CPU
    assert(parallel_default_jobs(-3) >= 1);          // and never less than one
    assert(parallel_default_jobs(0) <= 64);          // capped, so no fork bomb
    std::cout << "[PASS] test_default_jobs\n";
}

// ---------------------------------------------------------------------------
// JobManager: the %n / %str grammar used by wait, kill, fg and bg
// ---------------------------------------------------------------------------

static void test_job_specs() {
    JobManager jobs;
    assert(jobs.resolve("%1") == nullptr);   // empty table: nothing matches
    assert(jobs.active_job_count() == 0);

    // A "job" made of this very process, so nothing can be signalled by accident.
    const pid_t self = getpid();
    const int id = jobs.add_job(self, "sleep 3600", {self});
    assert(id == 1);
    assert(jobs.active_job_count() == 1);
    assert(jobs.job_ids() == std::vector<int>{1});
    assert(jobs.resolve("%1") != nullptr);
    assert(jobs.resolve("%") != nullptr);        // most recent job
    assert(jobs.resolve("%%") != nullptr);
    assert(jobs.resolve("1") != nullptr);        // bare number: job id, then pid
    assert(jobs.resolve("%sleep") != nullptr);   // by command prefix
    assert(jobs.resolve("%nonsense") == nullptr);
    assert(jobs.resolve("%2") == nullptr);

    Job* job = jobs.resolve("%1");
    assert(job != nullptr);
    assert(job->command_line == "sleep 3600");
    assert(job->state == JobState::RUNNING);
    assert(JobManager::state_label(*job) == "Running");

    jobs.remove_job(id);
    assert(jobs.active_job_count() == 0);
    assert(jobs.resolve("%1") == nullptr);
    std::cout << "[PASS] test_job_specs\n";
}

// ---------------------------------------------------------------------------
// The curated alias library
// ---------------------------------------------------------------------------

static void test_library_table() {
    const std::vector<AliasDef>& all = AliasLibrary::all();
    assert(all.size() > 30);   // a real library, not a token gesture

    std::vector<std::string> names;
    for (const auto& def : all) {
        assert(def.name && *def.name);
        assert(def.category && *def.category);
        assert(def.description != nullptr && *def.description);
        assert(def.definition != nullptr && *def.definition);
        assert(std::find(names.begin(), names.end(), std::string(def.name)) == names.end());
        names.push_back(def.name);
        const std::vector<std::string> cats = AliasLibrary::categories();
        assert(std::find(cats.begin(), cats.end(), std::string(def.category)) != cats.end());
        // Alias rows are one line; function rows must look like functions, so
        // `alias -p`, render() and the sourced file all stay honest.
        if (def.kind == AliasKind::ALIAS) {
            assert(std::string(def.definition).find('\n') == std::string::npos);
        } else {
            assert(std::string(def.definition).find("()") != std::string::npos ||
                   std::string(def.definition).find('[') != std::string::npos);
        }
    }
    std::cout << "[PASS] test_library_table (" << all.size() << " curated entries, "
              << AliasLibrary::categories().size() << " categories)\n";
}

static void test_library_expand() {
    std::vector<std::string> unknown;
    std::vector<const AliasDef*> files = AliasLibrary::for_category("files");
    assert(!files.empty());

    std::vector<const AliasDef*> mixed = AliasLibrary::expand({"ll", "ls-files"}, unknown);
    assert(mixed.size() == 1);   // `ll` exists, `ls-files` does not
    assert(unknown.size() == 1 && unknown[0] == "ls-files");

    unknown.clear();
    assert(AliasLibrary::expand({"files"}, unknown).size() == files.size());
    assert(unknown.empty());

    unknown.clear();
    assert(AliasLibrary::expand({"all"}, unknown).size() == AliasLibrary::all().size());

    // Comma lists and separate words select the same set, in table order, without
    // duplicates when a selection is mentioned twice.
    unknown.clear();
    size_t two_cats = AliasLibrary::for_category("files").size() + AliasLibrary::for_category("git").size();
    assert(AliasLibrary::expand({"files,git", "ll"}, unknown).size() == two_cats);
    unknown.clear();
    assert(AliasLibrary::expand({"files", "files"}, unknown).size() == files.size());
    assert(AliasLibrary::expand({}, unknown).empty());

    assert(AliasLibrary::find("ll") != nullptr);
    assert(AliasLibrary::find("no_such_alias") == nullptr);
    std::cout << "[PASS] test_library_expand\n";
}

static void test_install_roundtrip() {
    ScopedConfigDir dir;
    Environment env;
    JobManager jobs;
    Executor executor(env, jobs);

    std::vector<std::string> unknown;
    std::vector<const AliasDef*> defs = AliasLibrary::expand({"files", "up"}, unknown);
    assert(unknown.empty());
    assert(defs.size() == AliasLibrary::for_category("files").size() + 1);

    std::vector<std::string> skipped;
    std::vector<std::string> installed = AliasLibrary::install(env, &executor, defs, false, skipped);
    assert(installed.size() == defs.size());
    assert(skipped.empty());

    std::string value;
    assert(env.get_alias("ll", value) && value == "ls -lh");   // alias row applied
    assert(env.has_function("up"));                            // function row defined
    assert(executor.execute_string("up 0") == 0);              // and it runs

    // Persisted file: shell source a human would write.
    std::string err;
    assert(AliasLibrary::append_to_file(defs, err));
    std::string text = read_all(AliasLibrary::file_path());
    assert(text.find("alias ll='ls -lh'") != std::string::npos);
    assert(text.find("up()") != std::string::npos);
    assert(AliasLibrary::names_in_file().size() == defs.size());

    // Re-installing the same selection is idempotent, not duplicated.
    assert(AliasLibrary::append_to_file(defs, err));
    assert(AliasLibrary::names_in_file().size() == defs.size());
    std::string again = read_all(AliasLibrary::file_path());
    assert(again.find("alias ll=") == again.rfind("alias ll="));

    // A brand new shell picks the file up through source_into().
    Environment fresh_env;
    JobManager fresh_jobs;
    Executor fresh(fresh_env, fresh_jobs);
    assert(AliasLibrary::source_into(fresh) > 0);
    assert(fresh_env.get_alias("ll", value) && value == "ls -lh");
    assert(fresh_env.has_function("up"));

    // Uninstall removes both kinds, from the shell and from the file.
    AliasLibrary::uninstall(env, defs);
    assert(!env.get_alias("ll", value));
    assert(!env.has_function("up"));
    assert(AliasLibrary::remove_from_file(defs, err));
    assert(AliasLibrary::names_in_file().empty());
    std::cout << "[PASS] test_install_roundtrip\n";
}

static void test_install_conflicts_and_missing_tools() {
    ScopedConfigDir dir;
    Environment env;
    JobManager jobs;
    Executor executor(env, jobs);

    // A hand-written alias is never overwritten silently.
    env.set_alias("ll", "my own ls");
    std::vector<std::string> skipped;
    const size_t files_rows = AliasLibrary::for_category("files").size();
    std::vector<std::string> installed =
        AliasLibrary::install(env, &executor, AliasLibrary::for_category("files"), false, skipped);
    // Everything else installs; the one name the user already owns is left alone.
    assert(installed.size() == files_rows - 1);
    assert(skipped.size() == 1);
    assert(skipped[0].find("ll") != std::string::npos);
    std::string value;
    assert(env.get_alias("ll", value) && value == "my own ls");

    // ...unless the user asks for it.
    skipped.clear();
    installed = AliasLibrary::install(env, &executor, AliasLibrary::for_category("files"), true, skipped);
    assert(installed.size() == files_rows);
    assert(skipped.empty());
    assert(env.get_alias("ll", value) && value == "ls -lh");

    // A row that needs a tool this machine lacks is skipped, with a reason.
    AliasDef needs_row;
    needs_row.name = "asw_needs_test";
    needs_row.kind = AliasKind::ALIAS;
    needs_row.category = "sys";
    needs_row.definition = "echo hi";
    needs_row.description = "test row";
    needs_row.needs = "aswell_no_such_binary_on_this_box";
    skipped.clear();
    installed = AliasLibrary::install(env, &executor, {&needs_row}, true, skipped);
    assert(installed.empty());
    assert(skipped.size() == 1);
    assert(skipped[0].find("aswell_no_such_binary_on_this_box") != std::string::npos);

    // Functions still install when the executor is absent? No: nullptr means
    // "shell only", so function rows are reported as skipped instead.
    skipped.clear();
    installed = AliasLibrary::install(env, nullptr, AliasLibrary::for_category("nav"), false, skipped);
    size_t function_rows = 0;
    for (const AliasDef* def : AliasLibrary::for_category("nav"))
        if (def->kind == AliasKind::FUNCTION) function_rows++;
    assert(installed.size() + skipped.size() == AliasLibrary::for_category("nav").size());
    assert(skipped.size() >= function_rows);
    std::cout << "[PASS] test_install_conflicts_and_missing_tools\n";
}

static void test_render_is_valid_shell() {
    std::vector<std::string> unknown;
    std::vector<const AliasDef*> defs = AliasLibrary::expand({"files", "nav"}, unknown);
    const std::string text = AliasLibrary::render(defs);

    // Whatever preview() prints must parse and define the same names — that is
    // the contract of `aswell aliases preview git > file` + `source`.
    Environment env;
    JobManager jobs;
    Executor executor(env, jobs);
    assert(executor.execute_script(text) == 0);
    for (const AliasDef* def : defs) {
        std::string value;
        assert(env.get_alias(def->name, value) || env.has_function(def->name));
    }
    assert(text.find("# files\n") != std::string::npos);
    assert(text.find("alias ll='ls -lh'") != std::string::npos);
    std::cout << "[PASS] test_render_is_valid_shell\n";
}

static void test_remove_from_text() {
    std::vector<std::string> unknown;
    std::vector<const AliasDef*> defs = AliasLibrary::expand({"files"}, unknown);
    const std::string text = AliasLibrary::render(defs);

    // Dropping one entry keeps every other line intact.
    const AliasDef* victim = AliasLibrary::find("ll");
    assert(victim != nullptr);
    const std::string trimmed = AliasLibrary::remove_from_text(text, {victim});
    assert(trimmed.find("alias ll=") == std::string::npos);
    assert(trimmed.find("alias la=") != std::string::npos);
    assert(trimmed.find("alias lt=") != std::string::npos);

    // Names, not definitions, drive removal — a hand-edited alias still goes.
    const std::string edited = "alias ll='ls -l --color'\necho kept\n";
    assert(AliasLibrary::remove_names_from_text(edited, {"ll"}) == "echo kept\n");
    std::cout << "[PASS] test_remove_from_text\n";
}

// ---------------------------------------------------------------------------
// Reserved words as arguments (parser regression: `echo done` used to swallow
// the rest of the input because `done` was treated as control flow)
// ---------------------------------------------------------------------------

static void test_reserved_words_as_arguments() {
    Environment env;
    JobManager jobs;
    Executor executor(env, jobs);

    // Each of these must run every statement, in order, with the keyword as data.
    ASSERT_CAPTURED(executor, "echo done; echo after", "done\nafter\n");
    ASSERT_CAPTURED(executor, "if true; then echo fi; fi; echo tail", "fi\ntail\n");
    ASSERT_CAPTURED(executor, "for x in a b; do echo in-$x; done", "in-a\nin-b\n");
    ASSERT_CAPTURED(executor, "echo then; echo end", "then\nend\n");
    // A reserved word at the start of a command is still control flow, so the
    // loop bodies below must parse exactly as before.
    ASSERT_CAPTURED(executor, "while read -r line; do echo \"$line\"; done < /dev/null", "");
    ASSERT_CAPTURED(executor, "case x in done) echo A;; *) echo B;; esac", "B\n");
    ASSERT_CAPTURED(executor, "case done in a) echo A;; *) echo B;; esac", "B\n");
}

int main() {
    test_apply_template();
    test_split_lines();
    test_default_jobs();
    test_job_specs();
    test_library_table();
    test_library_expand();
    test_install_roundtrip();
    test_install_conflicts_and_missing_tools();
    test_render_is_valid_shell();
    test_remove_from_text();
    test_reserved_words_as_arguments();
    std::cout << "\nAll parallel & alias-library tests passed.\n";
    return 0;
}
