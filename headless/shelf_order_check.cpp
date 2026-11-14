// Host check of the shelf order (pe/ui/shelf_order.hpp).
// Build: c++ -std=c++20 -Ieglantineps5 shelf_order_check.cpp && ./a.out
#include "pe/ui/shelf_order.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

static std::vector<std::string> Ordered(std::vector<std::string> files,
                                        const std::vector<std::string> &favorites,
                                        const std::string &last,
                                        const std::vector<std::string> &recents)
{
    const auto rank = pe::ui::ShelfRanks(favorites, last, recents);
    std::stable_sort(files.begin(), files.end(), [&](const std::string &a, const std::string &b) {
        return pe::ui::ShelfRank(rank, a) < pe::ui::ShelfRank(rank, b);
    });
    return files;
}

int main()
{
    const std::vector<std::string> scan{"c.nsp", "a.nsp", "b.nsp", "d.nsp"};
    const auto plain = Ordered(scan, {}, "b.nsp", {"a.nsp", "c.nsp"});
    assert((plain == std::vector<std::string>{"b.nsp", "a.nsp", "c.nsp", "d.nsp"}));

    // A favorite stays ahead of the last game, and a second pin keeps its place.
    const auto pinned = Ordered(scan, {"d.nsp", "a.nsp"}, "b.nsp", {"c.nsp"});
    assert((pinned == std::vector<std::string>{"d.nsp", "a.nsp", "b.nsp", "c.nsp"}));

    // The last game is already a favorite: it is not listed twice.
    const auto once = Ordered(scan, {"b.nsp"}, "b.nsp", {"b.nsp", "a.nsp"});
    assert((once == std::vector<std::string>{"b.nsp", "a.nsp", "c.nsp", "d.nsp"}));

    // A pin whose file is gone does not open a hole. The games that remain keep their order.
    const auto missing = Ordered(scan, {"missing.nsp", "a.nsp"}, "b.nsp", {});
    assert((missing == std::vector<std::string>{"a.nsp", "b.nsp", "c.nsp", "d.nsp"}));

    std::cout << "shelf order: favorites, then last played, then recents PASS\n";
    return 0;
}
