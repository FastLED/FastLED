#include "fl/stl/cstdlib.h"
#include "test.h"

// Helper comparison function for integers
static int compare_ints(const void* a, const void* b) {
    int arg1 = *static_cast<const int*>(a);
    int arg2 = *static_cast<const int*>(b);

    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

// Helper comparison function for integers in reverse order
static int compare_ints_reverse(const void* a, const void* b) {
    int arg1 = *static_cast<const int*>(a);
    int arg2 = *static_cast<const int*>(b);

    if (arg1 > arg2) return -1;
    if (arg1 < arg2) return 1;
    return 0;
}

// Helper comparison function for doubles
static int compare_doubles(const void* a, const void* b) {
    double arg1 = *static_cast<const double*>(a);
    double arg2 = *static_cast<const double*>(b);

    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

// Helper struct for testing larger elements
struct Point {
    int x;
    int y;

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

// Comparison function for Points (sort by x, then y)
static int compare_points(const void* a, const void* b) {
    const Point* p1 = static_cast<const Point*>(a);
    const Point* p2 = static_cast<const Point*>(b);

    if (p1->x < p2->x) return -1;
    if (p1->x > p2->x) return 1;
    if (p1->y < p2->y) return -1;
    if (p1->y > p2->y) return 1;
    return 0;
}

FL_TEST_CASE("fl::qsort - basic functionality") {
    FL_SUBCASE("sort empty array") {
        int arr[1];
        fl::qsort(arr, 0, sizeof(int), compare_ints);
        // Should not crash
    }

    FL_SUBCASE("sort single element") {
        int arr[] = {42};
        fl::qsort(arr, 1, sizeof(int), compare_ints);
        FL_CHECK_EQ(arr[0], 42);
    }

    FL_SUBCASE("sort two elements - already sorted") {
        int arr[] = {1, 2};
        fl::qsort(arr, 2, sizeof(int), compare_ints);
        FL_CHECK_EQ(arr[0], 1);
        FL_CHECK_EQ(arr[1], 2);
    }

    FL_SUBCASE("sort two elements - reverse order") {
        int arr[] = {2, 1};
        fl::qsort(arr, 2, sizeof(int), compare_ints);
        FL_CHECK_EQ(arr[0], 1);
        FL_CHECK_EQ(arr[1], 2);
    }

    FL_SUBCASE("sort already sorted array") {
        int arr[] = {1, 2, 3, 4, 5};
        fl::qsort(arr, 5, sizeof(int), compare_ints);
        FL_CHECK_EQ(arr[0], 1);
        FL_CHECK_EQ(arr[1], 2);
        FL_CHECK_EQ(arr[2], 3);
        FL_CHECK_EQ(arr[3], 4);
        FL_CHECK_EQ(arr[4], 5);
    }

    FL_SUBCASE("sort reverse order") {
        int arr[] = {5, 4, 3, 2, 1};
        fl::qsort(arr, 5, sizeof(int), compare_ints);
        FL_CHECK_EQ(arr[0], 1);
        FL_CHECK_EQ(arr[1], 2);
        FL_CHECK_EQ(arr[2], 3);
        FL_CHECK_EQ(arr[3], 4);
        FL_CHECK_EQ(arr[4], 5);
    }

    FL_SUBCASE("sort random order") {
        int arr[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3};
        fl::qsort(arr, 10, sizeof(int), compare_ints);
        FL_CHECK_EQ(arr[0], 1);
        FL_CHECK_EQ(arr[1], 1);
        FL_CHECK_EQ(arr[2], 2);
        FL_CHECK_EQ(arr[3], 3);
        FL_CHECK_EQ(arr[4], 3);
        FL_CHECK_EQ(arr[5], 4);
        FL_CHECK_EQ(arr[6], 5);
        FL_CHECK_EQ(arr[7], 5);
        FL_CHECK_EQ(arr[8], 6);
        FL_CHECK_EQ(arr[9], 9);
    }
}

FL_TEST_CASE("fl::qsort - with duplicates") {
    FL_SUBCASE("all same elements") {
        int arr[] = {5, 5, 5, 5, 5};
        fl::qsort(arr, 5, sizeof(int), compare_ints);
        FL_CHECK_EQ(arr[0], 5);
        FL_CHECK_EQ(arr[1], 5);
        FL_CHECK_EQ(arr[2], 5);
        FL_CHECK_EQ(arr[3], 5);
        FL_CHECK_EQ(arr[4], 5);
    }

    FL_SUBCASE("many duplicates") {
        int arr[] = {3, 1, 3, 2, 1, 3, 2, 1};
        fl::qsort(arr, 8, sizeof(int), compare_ints);
        FL_CHECK_EQ(arr[0], 1);
        FL_CHECK_EQ(arr[1], 1);
        FL_CHECK_EQ(arr[2], 1);
        FL_CHECK_EQ(arr[3], 2);
        FL_CHECK_EQ(arr[4], 2);
        FL_CHECK_EQ(arr[5], 3);
        FL_CHECK_EQ(arr[6], 3);
        FL_CHECK_EQ(arr[7], 3);
    }
}

FL_TEST_CASE("fl::qsort - custom comparator") {
    FL_SUBCASE("reverse sort") {
        int arr[] = {1, 2, 3, 4, 5};
        fl::qsort(arr, 5, sizeof(int), compare_ints_reverse);
        FL_CHECK_EQ(arr[0], 5);
        FL_CHECK_EQ(arr[1], 4);
        FL_CHECK_EQ(arr[2], 3);
        FL_CHECK_EQ(arr[3], 2);
        FL_CHECK_EQ(arr[4], 1);
    }
}

FL_TEST_CASE("fl::qsort - different types") {
    FL_SUBCASE("sort doubles") {
        double arr[] = {3.14, 1.41, 2.71, 0.99, 4.20};
        fl::qsort(arr, 5, sizeof(double), compare_doubles);
        FL_CHECK_EQ(arr[0], 0.99);
        FL_CHECK_EQ(arr[1], 1.41);
        FL_CHECK_EQ(arr[2], 2.71);
        FL_CHECK_EQ(arr[3], 3.14);
        FL_CHECK_EQ(arr[4], 4.20);
    }

    FL_SUBCASE("sort struct") {
        Point arr[] = {
            {3, 5},
            {1, 2},
            {3, 1},
            {2, 4},
            {1, 7}
        };

        fl::qsort(arr, 5, sizeof(Point), compare_points);

        FL_CHECK((arr[0] == Point{1, 2}));
        FL_CHECK((arr[1] == Point{1, 7}));
        FL_CHECK((arr[2] == Point{2, 4}));
        FL_CHECK((arr[3] == Point{3, 1}));
        FL_CHECK((arr[4] == Point{3, 5}));
    }
}

FL_TEST_CASE("fl::qsort - larger arrays") {
    FL_SUBCASE("sort 100 elements descending") {
        int arr[100];
        for (int i = 0; i < 100; i++) {
            arr[i] = 100 - i;
        }

        fl::qsort(arr, 100, sizeof(int), compare_ints);

        for (int i = 0; i < 100; i++) {
            FL_CHECK_EQ(arr[i], i + 1);
        }
    }

    FL_SUBCASE("sort 1000 elements with pattern") {
        int arr[1000];
        // Create pattern: 500, 499, 498, ..., 1, 1000, 999, 998, ..., 501
        for (int i = 0; i < 500; i++) {
            arr[i] = 500 - i;
            arr[500 + i] = 1000 - i;
        }

        fl::qsort(arr, 1000, sizeof(int), compare_ints);

        for (int i = 0; i < 1000; i++) {
            FL_CHECK_EQ(arr[i], i + 1);
        }
    }
}

FL_TEST_CASE("fl::qsort - edge cases") {
    FL_SUBCASE("nullptr base") {
        fl::qsort(nullptr, 10, sizeof(int), compare_ints);
        // Should not crash
    }

    FL_SUBCASE("zero size") {
        int arr[] = {1, 2, 3};
        fl::qsort(arr, 3, 0, compare_ints);
        // Should not crash, but won't sort (undefined behavior, but safe)
    }

    FL_SUBCASE("nullptr comparator") {
        int arr[] = {1, 2, 3};
        fl::qsort(arr, 3, sizeof(int), nullptr);
        // Should not crash
    }
}

FL_TEST_CASE("fl::qsort - stress test") {
    FL_SUBCASE("sort with many swaps needed") {
        // Create array with alternating high/low values
        int arr[100];
        for (int i = 0; i < 50; i++) {
            arr[i * 2] = i + 50;      // High values
            arr[i * 2 + 1] = i;       // Low values
        }

        fl::qsort(arr, 100, sizeof(int), compare_ints);

        for (int i = 0; i < 100; i++) {
            FL_CHECK_EQ(arr[i], i);
        }
    }
}
