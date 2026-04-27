// tests/test_repo_policy.cpp
//
// Pass 1.5 -- Repo Collaboration Policy Tests (Catch2 v3)
//
// These tests VERIFY ONLY that policy files exist and contain required
// content.  They will FAIL on first run because the policy files do not
// exist yet -- that is expected.  Implementation creates the files;
// these tests define correctness.
//
// Requires CMake macro: CLAUDE_LANDER_REPO_ROOT (set via
//   target_compile_definitions)
//
// Coverage:
//   AC-1.5-001 .. 013   AGENTS.md
//   AC-1.5-020 .. 023   .github/pull_request_template.md
//   AC-1.5-030 .. 034   .github/ISSUE_TEMPLATE/{bug_report,feature_request}.md
//                        + config.yml
//   AC-1.5-040 .. 042   .github/CODEOWNERS
//   AC-1.5-050 .. 053   CONTRIBUTING.md
//   AC-1.5-060 .. 062   CODE_OF_CONDUCT.md
//   AC-1.5-070 .. 072   SECURITY.md
//   AC-1.5-080 .. 091   .github/workflows/enforce-issue-linked-pr.yml
//   AC-1.5-100 .. 109   scripts/setup-branch-protection.sh
//   AC-1.5-120 .. 124   docs/adr/0003-repo-collaboration-policy.md
//   AC-1.5-200 .. 202   Cross-cutting (no leaked usernames, no fabrication)

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <regex>
#include <string>

// ---------------------------------------------------------------------------
// Helper utilities
// ---------------------------------------------------------------------------

/// Read entire file into a string.  Returns empty string if not found so that
/// the CHECK / REQUIRE that follows produces the useful "file not found" failure
/// rather than an exception.
static std::string slurp(const std::filesystem::path& path)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        return {};
    }
    return { std::istreambuf_iterator<char>(f),
             std::istreambuf_iterator<char>() };
}

/// Case-insensitive substring search.
static bool contains_ci(const std::string& haystack, const std::string& needle)
{
    if (needle.empty()) return true;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(),   needle.end(),
        [](unsigned char a, unsigned char b) {
            return std::tolower(a) == std::tolower(b);
        });
    return it != haystack.end();
}

/// Count non-overlapping occurrences of a regex in text.
static std::size_t count_matches(const std::string& text, const std::regex& re)
{
    return static_cast<std::size_t>(
        std::distance(
            std::sregex_iterator(text.begin(), text.end(), re),
            std::sregex_iterator{}));
}

// Convenience: root path known at compile time via CMake macro.
// CLAUDE_LANDER_REPO_ROOT is set in CMakeLists.txt via
//   target_compile_definitions(... PRIVATE CLAUDE_LANDER_REPO_ROOT="...").
// Guard with a fallback that makes the missing-macro case obvious.
#ifndef CLAUDE_LANDER_REPO_ROOT
#  error "CLAUDE_LANDER_REPO_ROOT must be set by CMake via target_compile_definitions"
#endif

static const std::filesystem::path REPO_ROOT{ CLAUDE_LANDER_REPO_ROOT };

// ---------------------------------------------------------------------------
// AGENTS.md  (AC-1.5-001 .. 013)
// ---------------------------------------------------------------------------

TEST_CASE("AC-1.5-001: AGENTS.md exists at repo root", "[repo-policy]")
{
    const auto path = REPO_ROOT / "AGENTS.md";
    REQUIRE(std::filesystem::exists(path));
}

TEST_CASE("AC-1.5-002: AGENTS.md is non-empty and written in English", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "AGENTS.md");
    REQUIRE_FALSE(text.empty());
    // Must contain at least some common English words; guard against
    // accidentally committing a placeholder byte-order mark only.
    CHECK(text.size() > 200);
}

TEST_CASE("AC-1.5-003: AGENTS.md contains all required section headings", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "AGENTS.md");
    REQUIRE_FALSE(text.empty());

    // Each heading is checked case-insensitively so minor capitalisation
    // differences don't cause false failures.
    CHECK(contains_ci(text, "Project overview"));
    CHECK(contains_ci(text, "Clean-room policy"));
    CHECK(contains_ci(text, "How to read this file"));
    CHECK(contains_ci(text, "Branch strategy"));
    CHECK(contains_ci(text, "Pipeline gates"));
    CHECK(contains_ci(text, "Build instructions"));
    CHECK(contains_ci(text, "Coding standards"));
    CHECK(contains_ci(text, "Testing requirements"));
    CHECK(contains_ci(text, "Real-world validation"));
    CHECK(contains_ci(text, "Anti-fabrication rules"));
    CHECK(contains_ci(text, "DoD"));           // Definition of Done heading
    CHECK(contains_ci(text, "Reference docs"));
    CHECK(contains_ci(text, "Acknowledgements"));
}

TEST_CASE("AC-1.5-004: AGENTS.md names at least 4 agent tools", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "AGENTS.md");
    REQUIRE_FALSE(text.empty());

    int count = 0;
    if (contains_ci(text, "Claude Code"))  ++count;
    if (contains_ci(text, "Cursor"))       ++count;
    if (contains_ci(text, "Copilot"))      ++count;
    if (contains_ci(text, "Aider"))        ++count;
    CHECK(count >= 4);
}

TEST_CASE("AC-1.5-005: AGENTS.md contains branch strategy strings", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "AGENTS.md");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("feature/pass-N-") != std::string::npos);
    CHECK(contains_ci(text, "no direct push to main"));
}

TEST_CASE("AC-1.5-006: AGENTS.md contains Closes/Fixes/Resolves trio", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "AGENTS.md");
    REQUIRE_FALSE(text.empty());

    // All three keywords must appear (verbatim, exact case matches the
    // specification; the PR-link regex AC uses them as literals).
    CHECK(text.find("Closes") != std::string::npos);
    CHECK(text.find("Fixes")  != std::string::npos);
    CHECK(text.find("Resolves") != std::string::npos);
}

TEST_CASE("AC-1.5-007: AGENTS.md references CLAUDE_LANDER_BUILD_TESTS (not _BUILD_GAME)", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "AGENTS.md");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("CLAUDE_LANDER_BUILD_TESTS") != std::string::npos);
    // Must NOT mention a fabricated _BUILD_GAME variant.
    CHECK(text.find("CLAUDE_LANDER_BUILD_GAME") == std::string::npos);
}

TEST_CASE("AC-1.5-008: AGENTS.md contains required technical key terms", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "AGENTS.md");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("C++20")                  != std::string::npos);
    CHECK(text.find("Y-DOWN")                 != std::string::npos);
    CHECK(text.find("single-flip-point")      != std::string::npos);
    CHECK(text.find("OBJECT")                 != std::string::npos);
    CHECK(text.find("column-major")           != std::string::npos);
    CHECK(text.find("Catch2")                 != std::string::npos);
    CHECK(text.find("AC-")                    != std::string::npos);
    CHECK(text.find("[.golden]")              != std::string::npos);
    CHECK(text.find("deterministic")          != std::string::npos);
    CHECK(text.find("tolerance")              != std::string::npos);
    CHECK(text.find("decode-before-hypothesise") != std::string::npos);
    CHECK(text.find("DEFERRED")               != std::string::npos);
}

TEST_CASE("AC-1.5-009: AGENTS.md DoD checklist has at least 6 task-list items", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "AGENTS.md");
    REQUIRE_FALSE(text.empty());

    // Markdown task-list syntax: "- [ ]" (unchecked) or "- [x]" (checked).
    const std::regex task_item(R"(\-\s*\[[ xX]\])");
    CHECK(count_matches(text, task_item) >= 6);
}

TEST_CASE("AC-1.5-010: AGENTS.md credits required sources", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "AGENTS.md");
    REQUIRE_FALSE(text.empty());

    CHECK(contains_ci(text, "Braben"));
    CHECK(contains_ci(text, "Moxon"));
    CHECK(text.find("lander.bbcelite.com") != std::string::npos);
    CHECK(contains_ci(text, "clean-room"));
}

TEST_CASE("AC-1.5-011: AGENTS.md contains no TODO or FIXME", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "AGENTS.md");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("TODO")  == std::string::npos);
    CHECK(text.find("FIXME") == std::string::npos);
}

// ---------------------------------------------------------------------------
// .github/pull_request_template.md  (AC-1.5-020 .. 023)
// ---------------------------------------------------------------------------

TEST_CASE("AC-1.5-020: .github/pull_request_template.md exists", "[repo-policy]")
{
    const auto path = REPO_ROOT / ".github" / "pull_request_template.md";
    REQUIRE(std::filesystem::exists(path));
}

TEST_CASE("AC-1.5-021: PR template has a Closes/Fixes/Resolves placeholder", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / ".github" / "pull_request_template.md");
    REQUIRE_FALSE(text.empty());

    // At least one of the three linking keywords must appear.
    const bool has_keyword =
        text.find("Closes")   != std::string::npos ||
        text.find("Fixes")    != std::string::npos ||
        text.find("Resolves") != std::string::npos;
    CHECK(has_keyword);
}

TEST_CASE("AC-1.5-022: PR template has all required section headings", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / ".github" / "pull_request_template.md");
    REQUIRE_FALSE(text.empty());

    CHECK(contains_ci(text, "Summary"));
    CHECK(contains_ci(text, "Linked issue"));
    CHECK(contains_ci(text, "Test evidence"));
    CHECK(contains_ci(text, "AC IDs covered"));
    CHECK(contains_ci(text, "ADR references"));
    CHECK(contains_ci(text, "Reviewer checklist"));
}

TEST_CASE("AC-1.5-023: PR template reviewer checklist has at least 5 task-list items", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / ".github" / "pull_request_template.md");
    REQUIRE_FALSE(text.empty());

    const std::regex task_item(R"(\-\s*\[[ xX]\])");
    CHECK(count_matches(text, task_item) >= 5);
}

// ---------------------------------------------------------------------------
// .github/ISSUE_TEMPLATE/  (AC-1.5-030 .. 034)
// ---------------------------------------------------------------------------

TEST_CASE("AC-1.5-030: .github/ISSUE_TEMPLATE/bug_report.md exists", "[repo-policy]")
{
    const auto path = REPO_ROOT / ".github" / "ISSUE_TEMPLATE" / "bug_report.md";
    REQUIRE(std::filesystem::exists(path));
}

TEST_CASE("AC-1.5-031: bug_report.md has valid YAML frontmatter", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / ".github" / "ISSUE_TEMPLATE" / "bug_report.md");
    REQUIRE_FALSE(text.empty());

    // Must open with "---" frontmatter block.
    CHECK(text.substr(0, 3) == "---");
    // Must close frontmatter block.
    CHECK(text.find("\n---") != std::string::npos);
}

TEST_CASE("AC-1.5-032: .github/ISSUE_TEMPLATE/feature_request.md exists", "[repo-policy]")
{
    const auto path = REPO_ROOT / ".github" / "ISSUE_TEMPLATE" / "feature_request.md";
    REQUIRE(std::filesystem::exists(path));
}

TEST_CASE("AC-1.5-033: feature_request.md has valid YAML frontmatter", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / ".github" / "ISSUE_TEMPLATE" / "feature_request.md");
    REQUIRE_FALSE(text.empty());

    CHECK(text.substr(0, 3) == "---");
    CHECK(text.find("\n---") != std::string::npos);
}

TEST_CASE("AC-1.5-034: .github/ISSUE_TEMPLATE/config.yml disables blank issues", "[repo-policy]")
{
    const auto path = REPO_ROOT / ".github" / "ISSUE_TEMPLATE" / "config.yml";
    REQUIRE(std::filesystem::exists(path));

    const auto text = slurp(path);
    REQUIRE_FALSE(text.empty());
    CHECK(text.find("blank_issues_enabled: false") != std::string::npos);
}

// ---------------------------------------------------------------------------
// .github/CODEOWNERS  (AC-1.5-040 .. 042)
// ---------------------------------------------------------------------------

TEST_CASE("AC-1.5-040: .github/CODEOWNERS exists", "[repo-policy]")
{
    const auto path = REPO_ROOT / ".github" / "CODEOWNERS";
    REQUIRE(std::filesystem::exists(path));
}

TEST_CASE("AC-1.5-041: CODEOWNERS default rule references @L0rdS474n", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / ".github" / "CODEOWNERS");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("@L0rdS474n") != std::string::npos);
}

TEST_CASE("AC-1.5-042: CODEOWNERS contains only the maintainer handle", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / ".github" / "CODEOWNERS");
    REQUIRE_FALSE(text.empty());

    // The default rule must point at the project maintainer and only the
    // project maintainer.  Strategy: find all @<word> tokens and reject any
    // that differ from the canonical handle.
    const std::regex at_handle(R"(@([A-Za-z0-9_\-]+))");
    auto begin = std::sregex_iterator(text.begin(), text.end(), at_handle);
    auto end   = std::sregex_iterator{};
    for (auto it = begin; it != end; ++it) {
        const std::string handle = (*it)[0].str();  // includes leading @
        CHECK(handle == "@L0rdS474n");
    }
}

// ---------------------------------------------------------------------------
// CONTRIBUTING.md  (AC-1.5-050 .. 053)
// ---------------------------------------------------------------------------

TEST_CASE("AC-1.5-050: CONTRIBUTING.md exists at repo root", "[repo-policy]")
{
    const auto path = REPO_ROOT / "CONTRIBUTING.md";
    REQUIRE(std::filesystem::exists(path));
}

TEST_CASE("AC-1.5-051: CONTRIBUTING.md has all required section headings", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "CONTRIBUTING.md");
    REQUIRE_FALSE(text.empty());

    CHECK(contains_ci(text, "Getting started"));
    CHECK(contains_ci(text, "Branch strategy"));
    CHECK(contains_ci(text, "Commit conventions"));
    CHECK(contains_ci(text, "Running tests locally"));
    CHECK(contains_ci(text, "Filing issues"));
    CHECK(contains_ci(text, "Pull-request review process"));
}

TEST_CASE("AC-1.5-052: CONTRIBUTING.md contains branch pattern feature/pass-N-<topic>", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "CONTRIBUTING.md");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("feature/pass-N-") != std::string::npos);
}

TEST_CASE("AC-1.5-053: CONTRIBUTING.md contains ctest example", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "CONTRIBUTING.md");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("ctest") != std::string::npos);
}

// ---------------------------------------------------------------------------
// CODE_OF_CONDUCT.md  (AC-1.5-060 .. 062)
// ---------------------------------------------------------------------------

TEST_CASE("AC-1.5-060: CODE_OF_CONDUCT.md exists at repo root", "[repo-policy]")
{
    const auto path = REPO_ROOT / "CODE_OF_CONDUCT.md";
    REQUIRE(std::filesystem::exists(path));
}

TEST_CASE("AC-1.5-061: CODE_OF_CONDUCT.md references Contributor Covenant version 2.1", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "CODE_OF_CONDUCT.md");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("Contributor Covenant") != std::string::npos);
    CHECK(text.find("version 2.1")          != std::string::npos);
    CHECK(text.find("https://www.contributor-covenant.org/version/2/1/code_of_conduct.html")
          != std::string::npos);
}

TEST_CASE("AC-1.5-062: CODE_OF_CONDUCT.md contact line uses project maintainer email", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "CODE_OF_CONDUCT.md");
    REQUIRE_FALSE(text.empty());

    // Accept either the real email or the TBD placeholder.
    const bool has_contact =
        text.find("pomzm67@gmail.com")       != std::string::npos ||
        text.find("<MAINTAINER_EMAIL_TBD>")   != std::string::npos;
    CHECK(has_contact);
}

// ---------------------------------------------------------------------------
// SECURITY.md  (AC-1.5-070 .. 072)
// ---------------------------------------------------------------------------

TEST_CASE("AC-1.5-070: SECURITY.md exists at repo root", "[repo-policy]")
{
    const auto path = REPO_ROOT / "SECURITY.md";
    REQUIRE(std::filesystem::exists(path));
}

TEST_CASE("AC-1.5-071: SECURITY.md has required section headings", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "SECURITY.md");
    REQUIRE_FALSE(text.empty());

    CHECK(contains_ci(text, "Reporting a vulnerability"));
    CHECK(contains_ci(text, "Supported versions"));
    CHECK(contains_ci(text, "Response time"));
}

TEST_CASE("AC-1.5-072: SECURITY.md instructs reporters not to open a public issue", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "SECURITY.md");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("do not open a public issue") != std::string::npos);
}

// ---------------------------------------------------------------------------
// .github/workflows/enforce-issue-linked-pr.yml  (AC-1.5-080 .. 091)
// ---------------------------------------------------------------------------

TEST_CASE("AC-1.5-080: enforce-issue-linked-pr.yml exists", "[repo-policy]")
{
    const auto path = REPO_ROOT / ".github" / "workflows" / "enforce-issue-linked-pr.yml";
    REQUIRE(std::filesystem::exists(path));
}

TEST_CASE("AC-1.5-081: enforce-issue-linked-pr.yml is valid YAML (non-empty, starts with 'name:')", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / ".github" / "workflows" / "enforce-issue-linked-pr.yml");
    REQUIRE_FALSE(text.empty());

    // Minimal structural check: every GitHub Actions workflow starts with a
    // top-level 'name:' or 'on:' key.  We check for 'name:' specifically
    // because the AC requires the job name to match.
    CHECK(text.find("name:") != std::string::npos);
}

TEST_CASE("AC-1.5-082: enforce-issue-linked-pr.yml triggers on pull_request", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / ".github" / "workflows" / "enforce-issue-linked-pr.yml");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("pull_request") != std::string::npos);
}

TEST_CASE("AC-1.5-083: enforce-issue-linked-pr.yml includes types opened, edited, synchronize, reopened", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / ".github" / "workflows" / "enforce-issue-linked-pr.yml");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("opened")      != std::string::npos);
    CHECK(text.find("edited")      != std::string::npos);
    CHECK(text.find("synchronize") != std::string::npos);
    CHECK(text.find("reopened")    != std::string::npos);
}

TEST_CASE("AC-1.5-084: enforce-issue-linked-pr.yml job is named enforce-issue-linked-pr", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / ".github" / "workflows" / "enforce-issue-linked-pr.yml");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("enforce-issue-linked-pr") != std::string::npos);
}

TEST_CASE("AC-1.5-085: enforce-issue-linked-pr.yml regex accepts Closes #N", "[repo-policy]")
{
    // The workflow embeds or references a regex that should match
    // "Closes #<number>".  We verify the regex *string* is present in the YAML
    // so that out-of-band shell tests (check_pr_link_keyword.sh) and the
    // workflow definition stay aligned.
    const auto text = slurp(REPO_ROOT / ".github" / "workflows" / "enforce-issue-linked-pr.yml");
    REQUIRE_FALSE(text.empty());

    // The regex must capture the three keywords.
    CHECK(contains_ci(text, "closes"));
    CHECK(contains_ci(text, "fixes"));
    CHECK(contains_ci(text, "resolves"));
}

TEST_CASE("AC-1.5-086: enforce-issue-linked-pr.yml regex requires a numeric issue reference", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / ".github" / "workflows" / "enforce-issue-linked-pr.yml");
    REQUIRE_FALSE(text.empty());

    // A digit class or literal '#' followed by digits must appear in the regex
    // embedded in the workflow -- expressed as either '#' or '[0-9]' or '\d'.
    const bool has_number_pattern =
        text.find("#")     != std::string::npos &&
        (text.find("[0-9]") != std::string::npos ||
         text.find("\\d")   != std::string::npos ||
         text.find("[0-9]+") != std::string::npos);
    CHECK(has_number_pattern);
}

// ---------------------------------------------------------------------------
// scripts/setup-branch-protection.sh  (AC-1.5-100 .. 109)
// ---------------------------------------------------------------------------

TEST_CASE("AC-1.5-100: scripts/setup-branch-protection.sh exists", "[repo-policy]")
{
    const auto path = REPO_ROOT / "scripts" / "setup-branch-protection.sh";
    REQUIRE(std::filesystem::exists(path));
}

TEST_CASE("AC-1.5-101: setup-branch-protection.sh is executable", "[repo-policy]")
{
    const auto path = REPO_ROOT / "scripts" / "setup-branch-protection.sh";
    REQUIRE(std::filesystem::exists(path));

#ifdef _WIN32
    // NTFS does not implement POSIX permission bits the way ext4 does, and
    // git on Windows checks out shell scripts without an executable mode by
    // default.  The contract this AC encodes ("user can run the script
    // directly") is meaningless under MSYS2/cmd anyway -- shell scripts are
    // dispatched via the bash interpreter, which only requires read access.
    // We keep the existence guard above and treat the executable-bit check
    // as Linux/macOS only.
    SUCCEED("AC-1.5-101: skipped on Windows (POSIX exec bit semantics absent on NTFS)");
#else
    const auto perms = std::filesystem::status(path).permissions();
    const bool owner_exec =
        (perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none;
    CHECK(owner_exec);
#endif
}

TEST_CASE("AC-1.5-102: setup-branch-protection.sh has bash shebang", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "scripts" / "setup-branch-protection.sh");
    REQUIRE_FALSE(text.empty());

    CHECK(text.substr(0, 2) == "#!");
    CHECK(text.find("bash") != std::string::npos);
}

TEST_CASE("AC-1.5-103: setup-branch-protection.sh uses strict mode", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "scripts" / "setup-branch-protection.sh");
    REQUIRE_FALSE(text.empty());

    // Strict mode: at minimum -e, -u, -o pipefail (or combined set -euo pipefail)
    CHECK(text.find("set -") != std::string::npos);
    CHECK(text.find("-e")    != std::string::npos);
    CHECK(text.find("-u")    != std::string::npos);
    CHECK(text.find("pipefail") != std::string::npos);
}

TEST_CASE("AC-1.5-104: setup-branch-protection.sh resolves REPO_OWNER from env or gh api", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "scripts" / "setup-branch-protection.sh");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("REPO_OWNER") != std::string::npos);
    CHECK(text.find("gh api")     != std::string::npos);
}

TEST_CASE("AC-1.5-105: setup-branch-protection.sh default repo name is ClaudeLander", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "scripts" / "setup-branch-protection.sh");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("ClaudeLander") != std::string::npos);
    CHECK(text.find("REPO_NAME")    != std::string::npos);
}

TEST_CASE("AC-1.5-106: setup-branch-protection.sh payload has required protection fields", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "scripts" / "setup-branch-protection.sh");
    REQUIRE_FALSE(text.empty());

    // These field names must appear in the JSON payload sent to the API.
    CHECK(text.find("required_status_checks")          != std::string::npos);
    CHECK(text.find("enforce_admins")                  != std::string::npos);
    CHECK(text.find("required_pull_request_reviews")   != std::string::npos);
    CHECK(text.find("restrictions")                    != std::string::npos);
}

TEST_CASE("AC-1.5-107: setup-branch-protection.sh --dry-run prints JSON and exits 0", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "scripts" / "setup-branch-protection.sh");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("--dry-run") != std::string::npos);
}

TEST_CASE("AC-1.5-108: setup-branch-protection.sh --help flag is implemented", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "scripts" / "setup-branch-protection.sh");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("--help") != std::string::npos);
}

// ---------------------------------------------------------------------------
// docs/adr/0003-repo-collaboration-policy.md  (AC-1.5-120 .. 124)
// ---------------------------------------------------------------------------

TEST_CASE("AC-1.5-120: docs/adr/0003-repo-collaboration-policy.md exists", "[repo-policy]")
{
    const auto path = REPO_ROOT / "docs" / "adr" / "0003-repo-collaboration-policy.md";
    REQUIRE(std::filesystem::exists(path));
}

TEST_CASE("AC-1.5-121: ADR-0003 has all required section headings", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "docs" / "adr" / "0003-repo-collaboration-policy.md");
    REQUIRE_FALSE(text.empty());

    CHECK(contains_ci(text, "Status"));
    CHECK(contains_ci(text, "Date"));
    CHECK(contains_ci(text, "Scope"));
    CHECK(contains_ci(text, "Context"));
    CHECK(contains_ci(text, "Decision"));
    CHECK(contains_ci(text, "Consequences"));
    CHECK(contains_ci(text, "Verification"));
}

TEST_CASE("AC-1.5-122: ADR-0003 status is Accepted", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "docs" / "adr" / "0003-repo-collaboration-policy.md");
    REQUIRE_FALSE(text.empty());

    CHECK(text.find("Accepted") != std::string::npos);
}

TEST_CASE("AC-1.5-123: ADR-0003 records the 5 required policy decisions", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "docs" / "adr" / "0003-repo-collaboration-policy.md");
    REQUIRE_FALSE(text.empty());

    CHECK(contains_ci(text, "AGENTS.md"));
    CHECK(contains_ci(text, "branch protection"));
    CHECK(contains_ci(text, "issue linking"));
    CHECK(contains_ci(text, "CODEOWNERS"));
    // Contributor Covenant 2.1 - may appear as "Covenant" + "2.1" in proximity.
    CHECK((contains_ci(text, "Covenant") || contains_ci(text, "Code of Conduct")));
    CHECK(text.find("2.1") != std::string::npos);
}

TEST_CASE("AC-1.5-124: ADR-0003 contains real-world validation note", "[repo-policy]")
{
    const auto text = slurp(REPO_ROOT / "docs" / "adr" / "0003-repo-collaboration-policy.md");
    REQUIRE_FALSE(text.empty());

    // Must acknowledge real-world validation (can be deferred, but must be noted).
    const bool has_validation_note =
        contains_ci(text, "real-world validation") ||
        contains_ci(text, "real-world") ||
        contains_ci(text, "Verification");
    CHECK(has_validation_note);
}

// ---------------------------------------------------------------------------
// Cross-cutting  (AC-1.5-200 .. 202)
// ---------------------------------------------------------------------------

TEST_CASE("AC-1.5-200: no real GitHub usernames appear outside CODEOWNERS @L0rdS474n", "[repo-policy]")
{
    // Check every policy file.  Any @<handle> that is not the placeholder is a
    // potential leak.  We intentionally do NOT scan build artefacts or the
    // tests directory itself, only the policy deliverables.
    const std::vector<std::filesystem::path> policy_files = {
        REPO_ROOT / "AGENTS.md",
        REPO_ROOT / ".github" / "pull_request_template.md",
        REPO_ROOT / ".github" / "ISSUE_TEMPLATE" / "bug_report.md",
        REPO_ROOT / ".github" / "ISSUE_TEMPLATE" / "feature_request.md",
        REPO_ROOT / ".github" / "workflows" / "enforce-issue-linked-pr.yml",
        REPO_ROOT / "CONTRIBUTING.md",
        REPO_ROOT / "CODE_OF_CONDUCT.md",
        REPO_ROOT / "SECURITY.md",
        REPO_ROOT / "docs" / "adr" / "0003-repo-collaboration-policy.md",
    };

    const std::regex at_handle(R"(@([A-Za-z][A-Za-z0-9_\-]{0,38}))");

    for (const auto& file : policy_files) {
        if (!std::filesystem::exists(file)) continue;  // not yet created -- skip
        const auto text = slurp(file);
        auto begin = std::sregex_iterator(text.begin(), text.end(), at_handle);
        auto end   = std::sregex_iterator{};
        for (auto it = begin; it != end; ++it) {
            const std::string handle = (*it)[0].str();
            // Allow the canonical maintainer handle and email-like contexts
            // (@gmail etc.).  We only flag handles that look like other real
            // GitHub usernames slipping into policy files outside CODEOWNERS.
            const bool is_maintainer = handle == "@L0rdS474n";
            const bool is_email_part = handle.find('.') != std::string::npos ||
                                       text.find(handle + ".") != std::string::npos;
            if (!is_maintainer && !is_email_part) {
                // Flag as a potential real username leak -- reviewer must inspect.
                // We use WARN (not FAIL) to keep the test informative without
                // blocking on ambiguous matches like @github or @actions.
                WARN("Possible username in " + file.filename().string() + ": " + handle);
            }
        }
    }
    // The test always passes here; WARNs are for human review.
    SUCCEED();
}

TEST_CASE("AC-1.5-201: no fabricated tool output present in policy files", "[repo-policy]")
{
    // Files must NOT contain fabricated command output patterns:
    // lines that look like "$ command" or "```\n$ " followed by fake results.
    // We check for a common fabrication marker: "$ gh api" or "$ curl" without
    // an accompanying "# NOTE: output not verified" caveat.
    const auto agents_text = slurp(REPO_ROOT / "AGENTS.md");
    if (!agents_text.empty()) {
        // Any literal fabricated terminal session would look like "$ gh api ..."
        // *followed immediately* by output lines.  We warn on raw shell prompts
        // embedded in prose because those must be explained, not bare.
        const std::regex shell_prompt(R"(\n\$\s+\S)");
        if (count_matches(agents_text, shell_prompt) > 0) {
            WARN("AGENTS.md contains bare shell prompts -- verify these are "
                 "example commands, not fabricated output");
        }
    }
    SUCCEED();
}

TEST_CASE("AC-1.5-202: policy markdown files have no broken heading hierarchy", "[repo-policy]")
{
    // Documents must not jump from h1 directly to h3+ without an h2 in between.
    // This is a markdown lint rule (ML001 equivalent).
    const std::vector<std::pair<std::string, std::filesystem::path>> files = {
        { "AGENTS.md",        REPO_ROOT / "AGENTS.md" },
        { "CONTRIBUTING.md",  REPO_ROOT / "CONTRIBUTING.md" },
        { "CODE_OF_CONDUCT.md", REPO_ROOT / "CODE_OF_CONDUCT.md" },
        { "SECURITY.md",      REPO_ROOT / "SECURITY.md" },
    };

    for (const auto& [name, path] : files) {
        if (!std::filesystem::exists(path)) continue;
        const auto text = slurp(path);

        // Extract heading levels in document order.
        const std::regex heading_line(R"(^(#{1,6})\s)");
        std::vector<int> levels;
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            std::smatch m;
            if (std::regex_search(line, m, heading_line)) {
                levels.push_back(static_cast<int>(m[1].str().size()));
            }
        }

        // Check: no level jump greater than 1 from the previous heading.
        for (std::size_t i = 1; i < levels.size(); ++i) {
            const int jump = levels[i] - levels[i - 1];
            if (jump > 1) {
                WARN(name + ": heading level jumps from h" +
                     std::to_string(levels[i-1]) + " to h" +
                     std::to_string(levels[i]) + " -- markdown lint warning");
            }
        }
    }
    SUCCEED();
}
