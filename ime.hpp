#include <filesystem>
#include <string>
#include <unordered_map>

#include "dagdict.hpp"

class IME {
   public:
    IME(std::filesystem::path path_to_data);
    std::vector<std::string> candidates(const std::string& input);
    std::vector<std::string> split_words(const std::string& input) const;

   private:
    DAGDict codes;
    std::unordered_map<std::string, std::string> dict;
    std::unordered_map<std::string, std::vector<std::pair<std::string, size_t>>>
        vocabs;

    // struct ExpandRuleLess {
    //     bool operator()(const std::string& left, const std::string& right)
    //         const {
    //         return left.length() > right.length();
    //     }
    // };
    // std::map<std::string, std::string, ExpandRuleLess> expand_rules;
};
