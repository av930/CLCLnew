/*
 * CLCL
 *
 * Search.cpp
 *
 * Text search for the history search box (regex, falls back to plain substring match)
 */

#include "Search.h"
#include <regex>
#include <vector>
#include <cwctype>

/*
 * search_plain_match - case-insensitive plain substring search
 */
static void search_plain_match(const std::wstring &text, const std::wstring &pattern,
								std::vector<std::pair<int, int>> &found)
{
	std::wstring lower_text = text;
	std::wstring lower_pattern = pattern;

	for (auto &c : lower_text) {
		c = (wchar_t)std::towlower(c);
	}
	for (auto &c : lower_pattern) {
		c = (wchar_t)std::towlower(c);
	}

	size_t pos = 0;
	while ((pos = lower_text.find(lower_pattern, pos)) != std::wstring::npos) {
		found.push_back(std::make_pair((int)pos, (int)pattern.length()));
		pos += (pattern.length() > 0) ? pattern.length() : 1;
	}
}

/*
 * search_text_match - find matches using regex (falls back to plain text search if invalid)
 */
extern "C" BOOL search_text_match(const TCHAR *text, const TCHAR *pattern, SEARCH_MATCH **matches, int *match_cnt)
{
	*matches = NULL;
	*match_cnt = 0;
	if (text == NULL || pattern == NULL || *pattern == TEXT('\0')) {
		return FALSE;
	}

	std::wstring input(text);
	std::wstring query(pattern);
	std::vector<std::pair<int, int>> found;

	try {
		std::wregex re(query, std::regex_constants::icase);
		auto begin = std::wsregex_iterator(input.begin(), input.end(), re);
		auto end = std::wsregex_iterator();

		for (auto it = begin; it != end; ++it) {
			if (it->length() == 0) {
				continue;
			}
			found.push_back(std::make_pair((int)it->position(), (int)it->length()));
		}
	} catch (const std::regex_error &) {
		// Invalid regex pattern: fall back to plain substring search
		found.clear();
		search_plain_match(input, query, found);
	}

	if (found.empty()) {
		return FALSE;
	}

	*matches = (SEARCH_MATCH *)malloc(sizeof(SEARCH_MATCH) * found.size());
	if (*matches == NULL) {
		return FALSE;
	}
	for (size_t i = 0; i < found.size(); i++) {
		(*matches)[i].pos = found[i].first;
		(*matches)[i].len = found[i].second;
	}
	*match_cnt = (int)found.size();
	return TRUE;
}

/*
 * search_free_matches - free the buffer allocated by search_text_match
 */
extern "C" void search_free_matches(SEARCH_MATCH *matches)
{
	free(matches);
}
/* End of source */
