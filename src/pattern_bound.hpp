// Each word must have every short contiguous tile substring somewhere on the
// board. Compile wildcard path-pattern -> substring ID lists, then test word
// requirements once. This relaxes consistency between occurrences and words.
#pragma once

class PatternBound {
    struct Word {
        uint32_t first, inventory_first;
        uint8_t count, inventory_count, points;
    };
    struct Path { std::array<uint8_t, 6> cells{}; uint8_t length = 0; };
    int cells_, base_, length_;
    std::array<int, 26> digits_{};
    std::array<uint32_t, 7> power_{}, start_{};
    std::vector<uint32_t> offsets_, matches_, requirements_;
    std::vector<Word> words_;
    std::vector<Path> paths_;
    std::vector<std::pair<uint8_t, uint8_t>> inventories_;
    std::vector<uint8_t> universal_;
    size_t gram_count_ = 0, hash_size_ = 1;
    uint64_t total_points_ = 0;
public:
    struct Workspace {
        std::vector<uint32_t> possible, seen_keys;
        uint32_t generation = 0;
        size_t bytes() const { return (possible.size() + seen_keys.size()) * sizeof(uint32_t); }
    };
private:

    static std::string key(std::string s) {
        return std::min(s, std::string(s.rbegin(), s.rend()));
    }
    static std::vector<std::string> grams(const std::string& word, int length) {
        const size_t k = std::min(word.size(), static_cast<size_t>(length));
        std::vector<std::string> out;
        for (size_t i = 0; i + k <= word.size(); ++i) out.push_back(key(word.substr(i, k)));
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }
    std::vector<uint32_t> patterns(const std::string& gram) const {
        const size_t k = gram.size();
        std::vector<uint32_t> result;
        for (unsigned mask = 0; mask < (1u << k); ++mask) {
            uint32_t forward = 0, reverse = 0;
            for (size_t j = 0; j < k; ++j) {
                const uint32_t d = mask & (1u << j) ? static_cast<uint32_t>(base_ - 1)
                                                    : static_cast<uint32_t>(digits_[gram[j] - 'a']);
                forward += d * power_[j];
                reverse += d * power_[k - 1 - j];
            }
            result.push_back(start_[k] + std::min(forward, reverse));
        }
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }
    void make_paths(int cols) {
        std::array<uint16_t, 16> adjacent{};
        for (int i = 0; i < cells_; ++i) for (int j = 0; j < cells_; ++j)
            if (i != j && std::abs(i / cols - j / cols) <= 1 && std::abs(i % cols - j % cols) <= 1)
                adjacent[i] = uint16_t(adjacent[i] | (1u << j));
        Path path;
        const auto walk = [&](auto&& self, int cell, uint16_t used) -> void {
            // Reversal has the same set of possible canonical substrings.
            if (path.length >= 2 && path.cells[0] < cell) paths_.push_back(path);
            if (path.length == length_) return;
            uint16_t todo = uint16_t(adjacent[cell] & ~used);
            while (todo) {
                const unsigned next = static_cast<unsigned>(__builtin_ctz(todo));
                todo = uint16_t(todo & (todo - 1));
                path.cells[path.length++] = static_cast<uint8_t>(next);
                self(self, static_cast<int>(next), uint16_t(used | (1u << next)));
                --path.length;
            }
        };
        for (int i = 0; i < cells_; ++i) {
            path.cells[0] = static_cast<uint8_t>(i); path.length = 1;
            walk(walk, i, uint16_t(1u << i));
        }
    }
    static bool first_pattern(uint32_t value, Workspace& work) {
        // A small per-call hash set avoids rescanning long postings for repeated
        // wildcard patterns. Load is at most 1/2; no table-sized mutable scratch.
        const size_t mask = work.seen_keys.size() - 1;
        size_t slot = (uint64_t(value) * 2654435761u) & mask;
        while (work.seen_keys[slot] != std::numeric_limits<uint32_t>::max()) {
            if (work.seen_keys[slot] == value) return false;
            slot = (slot + 1) & mask;
        }
        work.seen_keys[slot] = value;
        return true;
    }
public:
    PatternBound(const Dictionary& dict, int rows, int cols, int length = 5,
                 size_t memory_limit = size_t{2048} * 1024 * 1024)
        : cells_(grid_size(rows, cols, 16)), base_(0), length_(length) {
        if (rows < 2 || cols < 2 || cells_ > 16) throw std::runtime_error("Pattern bounds support at most 16 cells");
        if (length < 2 || length > 6) throw std::runtime_error("--window must be between 2 and 6");
        digits_.fill(-1);
        for (const auto& word : dict.tile_words) if (static_cast<int>(word.size()) <= cells_)
            for (char c : word) digits_[c - 'a'] = 0;
        for (int& digit : digits_) if (digit == 0) digit = base_++;
        ++base_; // Last digit is wildcard. Letters outside this alphabet match nothing.
        power_[0] = 1;
        uint32_t entries = 0;
        for (int k = 1; k <= length; ++k) {
            power_[k] = power_[k - 1] * static_cast<uint32_t>(base_);
            if (k >= 2) { start_[k] = entries; entries += power_[k]; }
        }
        if ((uint64_t(entries) + 1) * sizeof(uint32_t) > memory_limit)
            throw std::runtime_error("Pattern index exceeds --table-mib budget");
        offsets_.assign(static_cast<size_t>(entries) + 1, 0);
        std::map<std::string, uint32_t> ids;
        std::vector<std::string> strings;
        for (size_t i = 0; i < dict.tile_words.size(); ++i) {
            const auto& word = dict.tile_words[i];
            if (static_cast<int>(word.size()) > cells_) continue;
            const auto required = grams(word, length);
            Word item{static_cast<uint32_t>(requirements_.size()), static_cast<uint32_t>(inventories_.size()),
                      static_cast<uint8_t>(required.size()), 0, dict.entries[i].points};
            for (int c = 0; c < 26; ++c) if (dict.entries[i].counts[c]) {
                inventories_.push_back({static_cast<uint8_t>(c), dict.entries[i].counts[c]});
                ++item.inventory_count;
            }
            total_points_ += item.points;
            for (const auto& gram : required) {
                auto found = ids.find(gram);
                if (found == ids.end()) {
                    const uint32_t id = static_cast<uint32_t>(strings.size());
                    found = ids.emplace(gram, id).first;
                    strings.push_back(gram);
                    for (uint32_t p : patterns(gram)) ++offsets_[p + 1];
                }
                requirements_.push_back(found->second);
            }
            words_.push_back(std::move(item));
        }
        uint64_t postings = 0;
        for (uint32_t value : offsets_) postings += value;
        if (postings > std::numeric_limits<uint32_t>::max() ||
            (uint64_t(offsets_.size()) + postings) * sizeof(uint32_t) > memory_limit)
            throw std::runtime_error("Pattern tables exceed --table-mib budget");
        for (size_t i = 1; i < offsets_.size(); ++i) offsets_[i] += offsets_[i - 1];
        matches_.resize(static_cast<size_t>(postings));
        // Fill backwards, temporarily turning ends into starts. Restore offsets
        // afterwards without allocating another ~57 MiB cursor array.
        for (size_t id = 0; id < strings.size(); ++id)
            for (uint32_t p : patterns(strings[id])) matches_[--offsets_[p + 1]] = static_cast<uint32_t>(id);
        for (size_t i = 0; i + 1 < offsets_.size(); ++i) offsets_[i] = offsets_[i + 1];
        offsets_.back() = static_cast<uint32_t>(postings);
        gram_count_ = strings.size();
        make_paths(cols);
        while (hash_size_ < paths_.size() * 2 + 1) hash_size_ *= 2;
        // Existence of an all-wildcard window makes every dictionary fragment
        // possible. A subset-closure lookup recognizes this case in constant time.
        universal_.resize(size_t{1} << cells_);
        for (const auto& path : paths_) if (path.length == length_) {
            unsigned mask = 0;
            for (int i = 0; i < length_; ++i) mask |= 1u << path.cells[i];
            universal_[mask] = 1;
        }
        for (int bit = 0; bit < cells_; ++bit)
            for (unsigned mask = 0; mask < universal_.size(); ++mask)
                if (mask & (1u << bit)) universal_[mask] |= universal_[mask ^ (1u << bit)];
        std::vector<uint32_t> frequency(gram_count_);
        for (uint32_t id : requirements_) ++frequency[id];
        for (const Word& word : words_)
            std::sort(requirements_.begin() + word.first, requirements_.begin() + word.first + word.count,
                      [&](uint32_t a, uint32_t b) { return frequency[a] < frequency[b]; });
    }

    Workspace workspace() const {
        return {std::vector<uint32_t>(gram_count_), std::vector<uint32_t>(hash_size_), 0};
    }

    uint64_t bound(const std::string& pattern, Workspace& work) const {
        if (static_cast<int>(pattern.size()) != cells_) throw std::runtime_error("Incorrect partial-board length");
        std::array<int, 16> board{};
        std::array<uint8_t, 26> supply{};
        int blanks = 0;
        unsigned blank_mask = 0;
        for (int i = 0; i < cells_; ++i) {
            const char c = pattern[i];
            if (c == '.') { board[i] = base_ - 1; ++blanks; blank_mask |= 1u << i; }
            else {
                if (c < 'a' || c > 'z') throw std::runtime_error("Use lowercase a-z or '.' for unset cells");
                board[i] = digits_[c - 'a']; ++supply[c - 'a'];
            }
        }
        if (work.possible.size() != gram_count_ || work.seen_keys.size() != hash_size_)
            throw std::runtime_error("Workspace does not match pattern tables");
        if (blanks == cells_) return total_points_;
        const bool all_possible = universal_[blank_mask] != 0;
        if (++work.generation == 0) { std::fill(work.possible.begin(), work.possible.end(), 0); ++work.generation; }
        std::fill(work.seen_keys.begin(), work.seen_keys.end(), std::numeric_limits<uint32_t>::max());
        size_t found = 0;
        if (!all_possible) for (const auto& path : paths_) {
            uint32_t forward = 0, reverse = 0;
            bool valid = true;
            for (unsigned j = 0; j < path.length; ++j) {
                const int d = board[path.cells[j]];
                if (d < 0) { valid = false; break; }
                forward += static_cast<uint32_t>(d) * power_[j];
                reverse += static_cast<uint32_t>(d) * power_[path.length - 1 - j];
            }
            if (!valid) continue;
            const uint32_t p = start_[path.length] + std::min(forward, reverse);
            if (!first_pattern(p, work)) continue;
            for (uint32_t i = offsets_[p]; i < offsets_[p + 1]; ++i) {
                const uint32_t id = matches_[i];
                if (work.possible[id] != work.generation) { work.possible[id] = work.generation; ++found; }
            }
            if (found == gram_count_) break;
        }
        uint64_t value = 0;
        for (const auto& word : words_) {
            bool possible = true;
            if (!all_possible) for (uint32_t i = word.first; i < word.first + word.count; ++i)
                if (work.possible[requirements_[i]] != work.generation) { possible = false; break; }
            if (!possible) continue;
            int missing = 0;
            for (uint32_t i = word.inventory_first; i < word.inventory_first + word.inventory_count; ++i) {
                const auto& count = inventories_[i];
                missing += std::max(0, int(count.second) - supply[count.first]);
                if (missing > blanks) break;
            }
            if (missing <= blanks) value += word.points;
        }
        return value;
    }
    size_t table_bytes() const { return (offsets_.size() + matches_.size()) * sizeof(uint32_t); }
    size_t paths() const { return paths_.size(); }
    size_t grams() const { return gram_count_; }
};
