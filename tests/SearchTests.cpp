#include "../Search.h"
#include <chrono>
#include <cstdio>
#include <initializer_list>
#include <regex>
#include <string>
#include <thread>

static bool expect(const wchar_t *text, const wchar_t *pattern,
    std::initializer_list<SEARCH_MATCH> expected)
{
    SEARCH_MATCH *matches = nullptr;
    int count = -1;
    BOOL matched = search_text_match(text, pattern, &matches, &count);
    bool passed = (matched != FALSE) == (expected.size() != 0) &&
        count == static_cast<int>(expected.size()) && (count != 0 || matches == nullptr);
    int index = 0;
    for (const auto &entry : expected) {
        passed = passed && matches != nullptr && matches[index].pos == entry.pos && matches[index].len == entry.len;
        index++;
    }
    search_free_matches(matches);
    return passed;
}

int main()
{
    bool passed = expect(L"Alpha alpha", L"alpha", {{0, 5}, {6, 5}}) &&
        expect(L"ALPHA", L"alpha", {{0, 5}}) &&
        expect(L"a12 B345", L"[a-z][0-9]+", {{0, 3}, {4, 4}}) &&
        expect(L"[a [A", L"[a", {{0, 2}, {3, 2}}) &&
        expect(L"[A", L"[a", {{0, 2}}) &&
        expect(L"alpha", L"alpha", {{0, 5}}) &&
        expect(L"alpha", L"beta", {}) &&
        expect(L"alpha", L"^|$", {}) &&
        expect(L"alpha", L"", {}) &&
        expect(nullptr, L"alpha", {}) && expect(L"alpha", nullptr, {});
    bool worker_passed = false;
    std::thread worker([&worker_passed] {
        worker_passed = expect(L"BETA beta", L"beta", {{0, 4}, {5, 4}}) &&
            expect(L"[", L"[", {{0, 1}});
    });
    worker.join();
    passed = passed && worker_passed && expect(L"alpha", L"alpha", {{0, 5}});
    if (!passed) {
        std::puts("[FAIL] Search regression");
        return 1;
    }
    std::puts("[OKAY] Search results, invalid patterns, query changes and thread isolation");

    const std::wstring text = L"Alpha 123 beta 456 gamma 789";
    const wchar_t *pattern = L"[a-z]+ [0-9]+";
    const int iterations = 5000;
    int baseline_count = 0, cached_count = 0;
    auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; iteration++) {
        std::wregex expression(pattern, std::regex_constants::icase);
        for (auto match = std::wsregex_iterator(text.begin(), text.end(), expression);
            match != std::wsregex_iterator(); ++match) {
            if (match->length() != 0) baseline_count++;
        }
    }
    auto baseline_end = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; iteration++) {
        SEARCH_MATCH *matches = nullptr;
        int count = 0;
        search_text_match(text.c_str(), pattern, &matches, &count);
        cached_count += count;
        search_free_matches(matches);
    }
    auto cached_end = std::chrono::steady_clock::now();
    std::printf("[INFO] %d calls: regex compilation %.2f ms, cached search %.2f ms\n", iterations,
        std::chrono::duration<double, std::milli>(baseline_end - start).count(),
        std::chrono::duration<double, std::milli>(cached_end - baseline_end).count());
    return baseline_count == cached_count ? 0 : 1;
}