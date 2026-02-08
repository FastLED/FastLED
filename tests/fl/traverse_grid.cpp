#include "fl/traverse_grid.h"

#include "fl/stl/set.h"
#include "fl/stl/new.h"
#include "fl/stl/utility.h"
#include "test.h"
#include "fl/geometry.h"


struct CollectingVisitor {
    fl::set<fl::pair<int, int>> visited;

    void visit(int x, int y) {
        visited.insert({x, y});
    }
};

FL_TEST_CASE("Traverse grid") {
    FL_SUBCASE("Horizontal line") {
        fl::vec2f start{1.2f, 2.5f};
        fl::vec2f end{5.7f, 2.5f};

        CollectingVisitor visitor;
        traverseGridSegment(start, end, visitor);

        FL_CHECK(visitor.visited.size() == 5);
        for (int x = 1; x <= 5; ++x) {
            FL_CHECK(visitor.visited.count({x, 2}) == 1);
        }
    }

    FL_SUBCASE("Vertical line") {
        fl::vec2f start{3.4f, 1.1f};
        fl::vec2f end{3.4f, 4.9f};

        CollectingVisitor visitor;
        traverseGridSegment(start, end, visitor);

        FL_CHECK(visitor.visited.size() == 4);
        for (int y = 1; y <= 4; ++y) {
            FL_CHECK(visitor.visited.count({3, y}) == 1);
        }
    }


    FL_SUBCASE("Forward diagonal") {
        fl::vec2f start{1.1f, 1.1f};
        fl::vec2f end{4.9f, 4.9f};

        CollectingVisitor visitor;
        traverseGridSegment(start, end, visitor);

        fl::set<fl::pair<int, int>> expected = {
            {1,1}, {1,2}, {2,2}, {2,3}, {3,3}, {3,4}, {4,4}
        };

        FL_CHECK(visitor.visited.size() == expected.size());
        for (const auto& cell : expected) {
            FL_CHECK(visitor.visited.count(cell) == 1);
        }
    }

    FL_SUBCASE("Backward diagonal") {
        fl::vec2f start{4.9f, 1.1f};
        fl::vec2f end{1.1f, 4.9f};

        CollectingVisitor visitor;
        traverseGridSegment(start, end, visitor);

        fl::set<fl::pair<int, int>> expected = {
            {4,1}, {4,2}, {3,2}, {3,3}, {2,3}, {2,4}, {1,4}
        };

        FL_CHECK(visitor.visited.size() == expected.size());
        for (const auto& cell : expected) {
            FL_CHECK(visitor.visited.count(cell) == 1);
        }
    }

    FL_SUBCASE("Single cell") {
        fl::vec2f start{2.2f, 3.3f};
        fl::vec2f end{2.2f, 3.3f};

        CollectingVisitor visitor;
        traverseGridSegment(start, end, visitor);

        FL_CHECK(visitor.visited.size() == 1);
        FL_CHECK(visitor.visited.count({2, 3}) == 1);
    }
}
