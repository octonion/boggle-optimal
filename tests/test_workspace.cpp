#define BOGGLE_OPTIMAL_NO_MAIN
#include "../src/main.cpp"
#include <cassert>

int main(int argc, char** argv) {
    assert(argc == 2);
    Dictionary dictionary(argv[1]);
    const PatternBound tables(dictionary, 2, 3, 3);
    auto a = tables.workspace();
    auto b = tables.workspace();
    const auto value = tables.bound("a.....", a);
    const auto other = tables.bound("..r...", b);
    for (int k = 2; k <= 6; ++k) {
        const PatternBound index(dictionary, 2, 3, k);
        auto mask_work = index.workspace(), scalar_work = index.workspace();
        uint32_t random = 451800;
        for (int test = 0; test < 128; ++test) {
            std::string partial(6, '.');
            for (char& c : partial) { random = random * 1664525u + 1013904223u; c = ".arqz"[(random >> 16) % 5]; }
            for (int cell = 0; cell < 6; ++cell) if (partial[cell] == '.') {
                const auto simultaneous = index.letter_bounds(partial, cell, mask_work);
                for (int a = 0; a < 26; ++a) {
                    auto child = partial; child[cell] = static_cast<char>('a' + a);
                    assert(simultaneous[a] == index.bound(child, scalar_work));
                }
                mask_work.generation = std::numeric_limits<uint32_t>::max();
                assert(simultaneous == index.letter_bounds(partial, cell, mask_work));
            }
        }
    }
    assert(value == tables.bound("a.....", b));
    assert(other == tables.bound("..r...", a));
    a.generation = std::numeric_limits<uint32_t>::max();
    assert(value == tables.bound("a.....", a));
    bool caught = false;
    try { PatternBound::Workspace invalid; tables.bound("a.....", invalid); }
    catch (const std::runtime_error&) { caught = true; }
    assert(caught);
    const PatternBound full_grid(dictionary, 4, 4, 5);
    assert(full_grid.paths() == 4484); // Undirected simple paths of lengths 2..5.
    auto full_work = full_grid.workspace();
    Scorer scorer(dictionary, 4, 4);
    assert(full_grid.bound("perslatgsine....", full_work) >= scorer.score("perslatgsineters"));
    // Test the CLI parser directly; suppressing main must not suppress coverage.
    assert(number("12", "test", 25) == 12);
    std::cout << "PASS: 26 simultaneous letter bounds vs separate scalar evaluations, shared tables, workspaces, generation wrap\n";
}
