// Exact Boggle scoring and a deliberately simple, certified partial-board bound.
// All board indexing is row major; 'q' represents the physical Qu tile.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int word_points(int length) {
    if (length < 3) return 0;
    if (length <= 4) return 1;
    if (length == 5) return 2;
    if (length == 6) return 3;
    if (length == 7) return 5;
    return 11;
}

struct Entry {
    std::array<uint8_t, 26> counts{};
    uint8_t tiles = 0;
    uint8_t points = 0;
};

struct Node {
    std::array<int32_t, 26> next{};
    int32_t word_id = -1;
    Node() { next.fill(-1); }
};

struct Dictionary {
    std::vector<Node> nodes{1};
    std::vector<Entry> entries;
    size_t rejected = 0;
    size_t duplicates = 0;

    // Normalize to tile letters. Every 'q' must be followed immediately by 'u'.
    static bool normalize(const std::string& raw, std::string& tiles, int& written) {
        tiles.clear();
        written = 0;
        for (size_t i = 0; i < raw.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(raw[i]);
            if (c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c - 'A' + 'a');
            if (c < 'a' || c > 'z') return false;
            if (c == 'q') {
                if (i + 1 == raw.size()) return false;
                unsigned char next = static_cast<unsigned char>(raw[i + 1]);
                if (next != 'u' && next != 'U') return false;
                ++i;
                written += 2;
            } else {
                ++written;
            }
            tiles.push_back(static_cast<char>(c));
        }
        return written >= 3 && tiles.size() <= 25;
    }

    void add(const std::string& raw) {
        std::string tiles;
        int length = 0;
        if (!normalize(raw, tiles, length)) { ++rejected; return; }
        int32_t node = 0;
        for (unsigned char c : tiles) {
            const int edge = c - 'a';
            int32_t child = nodes[node].next[edge];
            if (child < 0) {
                child = static_cast<int32_t>(nodes.size());
                nodes[node].next[edge] = child;
                nodes.emplace_back();
            }
            node = child;
        }
        if (nodes[node].word_id >= 0) { ++duplicates; return; }
        nodes[node].word_id = static_cast<int32_t>(entries.size());
        Entry e;
        e.tiles = static_cast<uint8_t>(tiles.size());
        e.points = static_cast<uint8_t>(word_points(length));
        for (unsigned char c : tiles) ++e.counts[c - 'a'];
        entries.push_back(e);
    }

    explicit Dictionary(const std::string& filename) {
        std::ifstream in(filename);
        if (!in) throw std::runtime_error("Cannot read dictionary: " + filename);
        std::string word;
        while (in >> word) add(word);
        if (entries.empty()) throw std::runtime_error("No valid words in dictionary");
    }
};

class Scorer {
    const Dictionary& dict_;
    int rows_, cols_, size_;
    std::vector<uint32_t> neighbors_;
    std::vector<uint32_t> seen_;
    uint32_t generation_ = 0;
    std::string board_;
    uint64_t score_ = 0;

    void visit(int cell, int32_t node, uint32_t used) {
        const int edge = board_[cell] - 'a';
        node = dict_.nodes[node].next[edge];
        if (node < 0) return;
        const uint32_t bit = uint32_t{1} << cell;
        used |= bit;
        const int id = dict_.nodes[node].word_id;
        if (id >= 0 && seen_[id] != generation_) {
            seen_[id] = generation_;
            score_ += dict_.entries[id].points;
        }
        uint32_t available = neighbors_[cell] & ~used;
        while (available) {
            const int next_cell = __builtin_ctz(available);
            available &= available - 1;
            visit(next_cell, node, used);
        }
    }

public:
    Scorer(const Dictionary& dict, int rows, int cols)
        : dict_(dict), rows_(rows), cols_(cols), size_(rows * cols),
          neighbors_(static_cast<size_t>(size_)), seen_(dict.entries.size(), 0) {
        if (rows < 2 || cols < 2 || size_ > 25)
            throw std::runtime_error("Grid must have at least 2 rows and columns, at most 25 cells");
        for (int i = 0; i < size_; ++i) {
            const int r = i / cols, c = i % cols;
            for (int j = 0; j < size_; ++j) {
                const int rr = j / cols, cc = j % cols;
                if (i != j && std::abs(rr - r) <= 1 && std::abs(cc - c) <= 1)
                    neighbors_[i] |= uint32_t{1} << j;
            }
        }
    }

    uint64_t score(const std::string& board) {
        if (static_cast<int>(board.size()) != size_)
            throw std::runtime_error("Incorrect board length: " + board);
        for (char c : board) if (c < 'a' || c > 'z')
            throw std::runtime_error("Board must contain lowercase a-z only");
        board_ = board;
        score_ = 0;
        ++generation_;
        if (generation_ == 0) { std::fill(seen_.begin(), seen_.end(), 0); ++generation_; }
        for (int i = 0; i < size_; ++i) visit(i, 0, 0);
        return score_;
    }

    // Certified relaxation: ignore adjacency, but retain the exact tile inventory.
    // For every feasible word, its letter-count demand must fit in the fixed tiles
    // plus the number of unset cells. Incompatible words are allowed to coexist.
    uint64_t inventory_bound(const std::string& pattern) {
        if (static_cast<int>(pattern.size()) != size_)
            throw std::runtime_error("Incorrect partial-board length");
        std::array<int, 26> supply{};
        int blanks = 0;
        for (char c : pattern) {
            if (c == '.') { ++blanks; continue; }
            if (c < 'a' || c > 'z') throw std::runtime_error("Use lowercase a-z or '.' for unset cells");
            ++supply[c - 'a'];
        }
        if (blanks == 0) return score(pattern);
        uint64_t upper = 0;
        for (const Entry& entry : dict_.entries) {
            if (entry.tiles > size_) continue;
            int shortfall = 0;
            for (int k = 0; k < 26; ++k) {
                if (entry.counts[k] > supply[k]) {
                    shortfall += entry.counts[k] - supply[k];
                    if (shortfall > blanks) break;
                }
            }
            if (shortfall <= blanks) upper += entry.points;
        }
        return upper;
    }
};

void usage() {
    std::cerr << "Usage: boggle-optimal <score|bound|bench> DICTIONARY [--rows N --cols N] BOARD...\n"
              << "  score: exact score for each complete board\n"
              << "  bound: certified inventory upper bound, with '.' denoting unfilled cells\n"
              << "  bench: repeat the first complete board 10000 times (score checksum printed)\n"
              << "Default grid is 4x4, boards are row-major lowercase letters, q denotes Qu.\n";
}

int main_impl(int argc, char** argv) {
    if (argc < 4) { usage(); return 2; }
    const std::string command = argv[1];
    if (command != "score" && command != "bound" && command != "bench") {
        usage(); return 2;
    }
    const std::string filename = argv[2];
    int rows = 4, cols = 4;
    std::vector<std::string> boards;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--rows" || a == "--cols") {
            if (++i >= argc) throw std::runtime_error("Missing grid dimension");
            const int v = std::stoi(argv[i]);
            if (a == "--rows") rows = v; else cols = v;
        } else boards.push_back(a);
    }
    if (boards.empty()) throw std::runtime_error("No board provided");
    Dictionary dict(filename);
    Scorer scorer(dict, rows, cols);
    if (command == "bench") {
        const auto start = std::chrono::steady_clock::now();
        uint64_t checksum = 0;
        constexpr int trials = 10000;
        for (int k = 0; k < trials; ++k) checksum += scorer.score(boards[0]);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "boards=" << trials << " seconds=" << seconds
                  << " boards_per_second=" << trials / seconds
                  << " checksum=" << checksum << '\n';
        return 0;
    }
    for (const std::string& board : boards) {
        const uint64_t value = command == "score" ? scorer.score(board) : scorer.inventory_bound(board);
        std::cout << board << " " << value << '\n';
    }
    return 0;
}
}  // namespace

int main(int argc, char** argv) {
    try { return main_impl(argc, argv); }
    catch (const std::exception& e) { std::cerr << "error: " << e.what() << '\n'; return 1; }
}
