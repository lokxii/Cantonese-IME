#include <algorithm>
#include <climits>
#include <fstream>
#include <ranges>
#include <string_view>

#include "utf8/utf8.h"

#include "ime.hpp"

namespace rv = std::views;
namespace ranges = std::ranges;

template <typename T>
struct flatten : ranges::range_adaptor_closure<flatten<T>> {
    flatten() {}
    constexpr std::vector<T> operator()(const ranges::range auto&& r) {
        std::vector<T> out;
        for (auto&& l : r) {
            out.insert(
                out.end(),
                std::make_move_iterator(l.cbegin()),
                std::make_move_iterator(l.cend()));
        }
        return out;
    }
};

const std::vector<std::string> initials = {
    "ng", "gw", "kw", "ch", "a", "i", "o", "u", "b", "p", "m", "f",
    "d",  "t",  "n",  "l",  "g", "k", "h", "w", "j", "s", "y", "*",
};

std::pair<DAGDict, std::unordered_map<std::string, std::string>>
codes_from_file(std::ifstream&& code_f) {
    std::string line;
    std::vector<std::pair<std::string, std::vector<std::string>>> codes;
    std::unordered_map<std::string, std::string> dict;

    while (std::getline(code_f, line)) {
        auto it = std::find(line.cbegin(), line.cend(), ' ');
        auto key = std::string(line.cbegin(), it);
        it++;

        std::vector<std::string> chars;
        while (std::distance(line.cbegin(), it) < line.length()) {
            std::string c;
            utf8::append(utf8::next(it, line.cend()), std::back_inserter(c));
            chars.push_back(c);
            it++;
        }
        codes.push_back(std::make_pair(key, chars));
    }
    dict = codes | rv::transform([](const auto& p) {
               return p.second | rv::transform([&](const auto& w) {
                          return std::make_pair(w, p.first);
                      });
           }) |
           flatten<std::pair<std::string, std::string>>() |
           ranges::to<std::unordered_map>();
    return std::make_pair(DAGDict(codes), std::move(dict));
}

// template <typename T>
// std::map<std::string, std::string, T> expand_rules_from_file(
//     std::ifstream&& expand_rules_f) {
//     std::map<std::string, std::string, T> out;
//     std::string line;
//     while (std::getline(expand_rules_f, line, ',')) {
//         std::string alternative, fix;
//         std::stringstream ss(line);
//         ss >> fix >> alternative;
//         out.insert({fix, alternative});
//     }
//     return out;
// }

std::unordered_map<std::string, std::vector<std::pair<std::string, size_t>>>
vocab_from_file(std::ifstream&& words_f) {
    std::unordered_map<std::string, std::vector<std::pair<std::string, size_t>>>
        out;

    std::string line;
    while (std::getline(words_f, line)) {
        auto it = line.cbegin();
        std::string head, body;
        size_t freq;

        utf8::append(utf8::next(it, line.cend()), std::back_inserter(head));
        auto f_it = std::find(it, line.cend(), ',');
        body = std::string(it, f_it);
        it = f_it + 1;
        freq = std::stoi(std::string(it, line.cend()));

        if (out.contains(head)) {
            out[head].push_back(std::make_pair(head + body, freq));
        } else {
            out[head] = {std::make_pair(head + body, freq)};
        }
    }
    return out;
}

IME::IME(std::filesystem::path path_to_data) {
    auto code_f = std::ifstream(path_to_data / "codecantonese.csv");
    auto [codes, dict] = codes_from_file(std::move(code_f));
    this->codes = std::move(codes);
    this->dict = std::move(dict);

    auto expand_rules_f = std::ifstream(path_to_data / "codeexpand.csv");
    // this->expand_rules =
    //     expand_rules_from_file<ExpandRuleLess>(std::move(expand_rules_f));

    auto words_f = std::ifstream(path_to_data / "wordfreq.csv");
    this->vocabs = vocab_from_file(std::move(words_f));
}

std::vector<std::string> IME::split_words(const std::string& input) const {
    std::vector<std::string> out;
    auto it = input.cbegin();
    while (std::distance(input.cbegin(), it) < input.length()) {
        std::string prefix;
        for (const auto& initial : initials) {
            if (std::string_view(it, input.cend()).starts_with(initial)) {
                prefix = initial;
                break;
            }
        }
        if (prefix == "") {
            out.push_back(std::string(1, *it));
            it += 1;
            continue;
        }

        auto possible_keys = codes.possible_keys(prefix);
        std::sort(
            possible_keys.begin(),
            possible_keys.end(),
            [](const std::string& l, const std::string& r) {
                return l.length() > r.length();
            });

        auto view = std::string_view(it, input.end());
        auto word_it = std::find_if(
            possible_keys.cbegin(),
            possible_keys.cend(),
            [&](const std::string& key) {
                return key.length() < view.length() ? view.starts_with(key)
                                                    : key.starts_with(view);
            });
        if (word_it != possible_keys.cend()) {
            if (word_it->length() < view.length()) {
                out.push_back(*word_it);
                it += (*word_it).length();
            } else {
                out.push_back(std::string(view));
                it += view.length();
            }
            continue;
        }
        auto indicies =
            possible_keys | rv::transform([&](const auto& key) {
                int i = 1;
                for (; i <= key.length() && i <= view.length(); i++) {
                    if (key.substr(0, i) != view.substr(0, i)) {
                        break;
                    }
                }
                return i - 1;
            }) |
            ranges::to<std::vector>();
        auto max = *ranges::max_element(indicies);
        out.emplace_back(it, it + max);
        it += max;
    }
    return out;
}

std::vector<std::string> IME::candidates(const std::string& input) {
    // TODO: replace wrong input... HOW?
    auto words = split_words(input);
    if (words.empty()) {
        return {};
    }

    auto c1 = codes.get_all(words[0]) | rv::transform([&](const auto& head) {
                  return vocabs.contains(head)
                             ? vocabs[head]
                             : std::vector<std::pair<std::string, size_t>>();
              }) |
              flatten<std::pair<std::string, size_t>>();
    for (int i = 1; i < words.size(); i++) {
        auto sub_candidate = codes.get_all(words[i]);
        c1 = c1 | rv::filter([&](const auto& p) {
                 return utf8::distance(p.first.cbegin(), p.first.cend()) > i;
             }) |
             rv::filter([&](const auto& p) {
                 auto s = p.first.cbegin();
                 utf8::advance(s, i, p.first.cend());
                 auto e = s;
                 utf8::next(e, p.first.cend());
                 return std::find(
                            sub_candidate.cbegin(),
                            sub_candidate.cend(),
                            std::string_view(s, e)) != sub_candidate.cend();
             }) |
             ranges::to<std::vector>();
    }

    auto c2 = c1 | rv::transform([&](auto&& p) {
                  std::string last_char;
                  auto it = p.first.cend();
                  utf8::append(
                      utf8::prior(it, p.first.cbegin()),
                      std::back_inserter(last_char));
                  auto spell = dict[last_char];
                  auto dist = spell.starts_with(words.back())
                                  ? spell.length() - words.back().length()
                                  : ULONG_MAX;
                  return std::make_tuple(
                      std::move(p.first), std::move(p.second), dist);
              }) |
              ranges::to<std::vector>();
    c1.clear();

    std::sort(c2.begin(), c2.end(), [](const auto& l, const auto& r) {
        return std::get<1>(l) > std::get<1>(r);
    });
    std::stable_sort(c2.begin(), c2.end(), [](const auto& l, const auto& r) {
        return utf8::distance(std::get<0>(l).cbegin(), std::get<0>(l).cend()) <
               utf8::distance(std::get<0>(r).cbegin(), std::get<0>(r).cend());
    });
    std::stable_sort(c2.begin(), c2.end(), [](const auto& l, const auto& r) {
        return std::get<2>(l) < std::get<2>(r);
    });

    return c2 | rv::transform([](const auto& t) { return std::get<0>(t); }) |
           ranges::to<std::vector>();
}
