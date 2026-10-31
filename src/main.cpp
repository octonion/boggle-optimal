// Exact Boggle scoring, inventory bounds, and compiled fragment-table bounds.
// All board indexing is row major; 'q' represents the physical Qu tile.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int grid_size(int rows, int cols, int limit = 25) {
    if (rows < 2 || cols < 2 || rows > limit || cols > limit || rows > limit / cols)
        throw std::runtime_error("Grid dimensions exceed supported limits");
    return rows * cols;
}

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
    std::vector<std::string> tile_words;
    size_t rejected = 0;
    size_t duplicates = 0;

    // Normalize to tile letters. Every 'q' must be followed immediately by 'u'.
    static bool normalize(const std::string& raw, std::string& tiles, int& written) {
        tiles.clear();
        written = 0;
        if (raw.size() > 50) return false; // At most 25 tiles, each at most two written letters.
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
        tile_words.push_back(tiles);
    }

    explicit Dictionary(const std::string& filename) {
        std::ifstream in(filename);
        if (!in) throw std::runtime_error("Cannot read dictionary: " + filename);
        std::string word;
        while (in >> word) add(word);
        if (entries.empty()) throw std::runtime_error("No valid words in dictionary");
    }
};

#include "pattern_bound.hpp"

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
        : dict_(dict), rows_(rows), cols_(cols), size_(grid_size(rows, cols)),
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

uint64_t number(const std::string& value, const std::string& name, uint64_t limit) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("Invalid " + name + ": " + value);
    const uint64_t n = std::stoull(value);
    if (n > limit) throw std::runtime_error(name + " is too large");
    return n;
}

void usage() {
    std::cerr << "Usage: boggle-optimal <score|bound|bench|bench-bound> DICTIONARY [OPTIONS] BOARD...\n"
              << "  --rows N --cols N       Grid dimensions (default 4x4)\n"
              << "  --method inventory|patterns  Bound method (default inventory)\n"
              << "  --window N             Fragment length 2..6 (default 5)\n"
              << "  --table-mib N          Index + postings budget (default 2048 MiB)\n"
              << "  --force                Synchronize one unfilled cell across all words\n"
              << "  --force-cell N         Synchronize this zero-based cell instead\n"
              << "  --stdin                Also read boards from standard input\n"
              << "  --repeat N             Benchmark repetitions (default 1 for bench-bound)\n"
              << "  --threshold N          Incumbent score for pruning counts (default 3625)\n"
              << "Boards are row-major lowercase letters; q denotes Qu; '.' is an unfilled cell.\n"
              << "Pattern bounds support at most 16 cells. Complete boards are scored exactly.\n";
}

[[maybe_unused]] int main_impl(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") { usage(); return 0; }
    if (argc < 3) { usage(); return 2; }
    const std::string command = argv[1];
    if (command != "score" && command != "bound" && command != "bench" && command != "bench-bound") {
        usage(); return 2;
    }
    int rows = 4, cols = 4, window = 5;
    uint64_t table_mib = 2048, repeat = command == "bench" ? 10000 : 1, threshold = 3625;
    std::string method = "inventory";
    bool use_stdin = false, force = false;
    int force_cell = -1;
    std::vector<std::string> boards;
    for (int i = 3; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--force") { force = true; continue; }
        if (option == "--stdin") { use_stdin = true; continue; }
        if (option == "--method" || option == "--rows" || option == "--cols" || option == "--window" ||
            option == "--force-cell" || option == "--table-mib" || option == "--repeat" || option == "--threshold") {
            if (++i >= argc) throw std::runtime_error("Missing value for " + option);
            const std::string value = argv[i];
            if (option == "--method") { method = value; continue; }
            if (option == "--force-cell") { force_cell = static_cast<int>(number(value, option, 15)); force = true; }
            else if (option == "--rows") rows = static_cast<int>(number(value, option, 25));
            else if (option == "--cols") cols = static_cast<int>(number(value, option, 25));
            else if (option == "--window") window = static_cast<int>(number(value, option, 6));
            else if (option == "--repeat") repeat = number(value, option, 1000000);
            else if (option == "--threshold") threshold = number(value, option, std::numeric_limits<uint64_t>::max());
            else table_mib = number(value, option, std::numeric_limits<size_t>::max() / (1024 * 1024));
        } else {
            if (option.rfind("--", 0) == 0) throw std::runtime_error("Unknown option: " + option);
            boards.push_back(option);
        }
    }
    if (use_stdin) {
        std::string board;
        while (std::cin >> board) boards.push_back(board);
        if (std::cin.bad()) throw std::runtime_error("Cannot read boards from stdin");
    }
    if (boards.empty()) throw std::runtime_error("No board provided");
    if (rows < 2 || cols < 2 || rows * cols > 25) throw std::runtime_error("Invalid grid dimensions");
    if (window < 2 || !table_mib || !repeat) throw std::runtime_error("Window, table budget or repeat is out of range");
    if (method != "inventory" && method != "patterns") throw std::runtime_error("Unknown bound method");
    if (force && (method != "patterns" || (command != "bound" && command != "bench-bound")))
        throw std::runtime_error("Forcing requires --method patterns with bound or bench-bound");
    const bool bounding = command == "bound" || command == "bench-bound";
    Dictionary dict(argv[2]);
    Scorer scorer(dict, rows, cols);
    std::unique_ptr<PatternBound> patterns;
    PatternBound::Workspace work;
    double setup_seconds = 0;
    if (bounding && method == "patterns") {
        const auto start = std::chrono::steady_clock::now();
        patterns = std::make_unique<PatternBound>(dict, rows, cols, window,
                    static_cast<size_t>(table_mib) * 1024 * 1024);
        work = patterns->workspace();
        setup_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }
    const auto evaluate = [&](const std::string& board) {
        if (force_cell >= 0 && (force_cell >= static_cast<int>(board.size()) || board[force_cell] != '.'))
            throw std::runtime_error("Forced cell must be an unfilled cell of the board");
        if (!bounding || board.find('.') == std::string::npos) return scorer.score(board);
        if (!patterns) return scorer.inventory_bound(board);
        return force ? patterns->forced_bound(board, work, force_cell) : patterns->bound(board, work);
    };
    if (command == "bench" || command == "bench-bound") {
        const auto start = std::chrono::steady_clock::now();
        uint64_t checksum = 0, pruned = 0;
        for (uint64_t r = 0; r < repeat; ++r) for (const std::string& board : boards) {
            const uint64_t value = evaluate(board);
            checksum += value;
            if (value <= threshold) ++pruned;
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << std::setprecision(9) << "{\"boards\":" << boards.size() << ",\"repeat\":" << repeat
                  << ",\"seconds\":" << seconds << ",\"setup_seconds\":" << setup_seconds
                  << ",\"checksum\":" << checksum << ",\"pruned\":" << pruned
                  << ",\"threshold\":" << threshold << ",\"method\":\"" << (bounding ? method : "score")
                  << "\",\"force\":" << (force ? "true" : "false") << ",\"window\":" << window << ",\"table_bytes\":" << (patterns ? patterns->table_bytes() : 0)
                  << ",\"workspace_bytes\":" << work.bytes() << "}\n";
        return 0;
    }
    if (patterns) std::cerr << "table_bytes=" << patterns->table_bytes() << " workspace_bytes=" << work.bytes()
                            << " paths=" << patterns->paths() << " fragments=" << patterns->grams()
                            << " setup_seconds=" << setup_seconds << '\n';
    for (const std::string& board : boards) std::cout << board << " " << evaluate(board) << '\n';
    return 0;
}
}  // namespace

#ifndef BOGGLE_OPTIMAL_NO_MAIN
int main(int argc, char** argv) {
    try { return main_impl(argc, argv); }
    catch (const std::exception& e) { std::cerr << "error: " << e.what() << '\n'; return 1; }
}
#endif
