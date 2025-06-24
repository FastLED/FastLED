#include "test.h"
#include "fl/thread_local.h"
#include "fl/vector.h"
#include "fl/string.h"

#if FASTLED_USE_THREAD_LOCAL
#include <pthread.h>
#include <unistd.h>  // for usleep
#endif

using namespace fl;

TEST_CASE("ThreadLocal - basic functionality") {
    ThreadLocal<int> tls;
    
    // Default constructed value should be 0
    REQUIRE(tls.access() == 0);
    
    // Set a value
    tls.set(42);
    REQUIRE(tls.access() == 42);
    
    // Use assignment operator
    tls = 100;
    REQUIRE(tls.access() == 100);
    
    // Use conversion operator
    int value = tls;
    REQUIRE(value == 100);
}

TEST_CASE("ThreadLocal - with default value") {
    ThreadLocal<int> tls(999);
    
    // Should start with default value
    REQUIRE(tls.access() == 999);
    
    // Set a different value
    tls.set(123);
    REQUIRE(tls.access() == 123);
}

TEST_CASE("ThreadLocal - with custom type") {
    struct TestStruct {
        int value = 0;
        fl::string name = "default";
        
        TestStruct() = default;
        TestStruct(int v, const fl::string& n) : value(v), name(n) {}
        
        bool operator==(const TestStruct& other) const {
            return value == other.value && name == other.name;
        }
    };
    
    ThreadLocal<TestStruct> tls;
    
    // Default constructed
    REQUIRE(tls.access().value == 0);
    REQUIRE(tls.access().name == "default");
    
    // Set a value
    TestStruct custom(42, "test");
    tls.set(custom);
    REQUIRE(tls.access() == custom);
    
    // Modify in place
    tls.access().value = 99;
    tls.access().name = "modified";
    REQUIRE(tls.access().value == 99);
    REQUIRE(tls.access().name == "modified");
}

struct ThreadTestData {
    ThreadLocal<int>* tls;
    int thread_id;
    int expected_value;
    volatile bool ready;
    volatile bool done;
    volatile bool success;
    
    ThreadTestData() : tls(nullptr), thread_id(0), expected_value(0), 
                      ready(false), done(false), success(false) {}
};

#if FASTLED_USE_THREAD_LOCAL

static void* thread_test_func(void* arg) {
    ThreadTestData* data = static_cast<ThreadTestData*>(arg);
    
    // Wait for test to be ready
    while (!data->ready) {
        usleep(1000); // 1ms
    }
    
    // Set thread-specific value
    data->tls->set(data->expected_value);
    
    // Small delay to ensure other threads have a chance to interfere
    usleep(10000); // 10ms
    
    // Verify the value is still correct (thread isolation)
    bool success = (data->tls->access() == data->expected_value);
    data->success = success;
    data->done = true;
    
    return nullptr;
}

TEST_CASE("ThreadLocal - thread isolation") {
    ThreadLocal<int> tls;
    
    const int num_threads = 4;
    pthread_t threads[num_threads];
    ThreadTestData thread_data[num_threads];
    
    // Initialize thread data
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].tls = &tls;
        thread_data[i].thread_id = i;
        thread_data[i].expected_value = (i + 1) * 100; // 100, 200, 300, 400
    }
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        int result = pthread_create(&threads[i], nullptr, thread_test_func, &thread_data[i]);
        REQUIRE(result == 0);
    }
    
    // Signal threads to start
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].ready = true;
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        while (!thread_data[i].done) {
            usleep(1000); // 1ms
        }
    }
    
    // Join threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], nullptr);
    }
    
    // Verify all threads succeeded
    for (int i = 0; i < num_threads; i++) {
        REQUIRE(thread_data[i].success);
    }
    
    // Main thread should still have default value
    REQUIRE(tls.access() == 0);
}

struct SharedTLSTestData {
    ThreadLocal<int>* tls1;
    ThreadLocal<int>* tls2;
    int thread_id;
    volatile bool ready;
    volatile bool done;
    volatile bool success;
    
    SharedTLSTestData() : tls1(nullptr), tls2(nullptr), thread_id(0),
                         ready(false), done(false), success(false) {}
};

static void* shared_tls_test_func(void* arg) {
    SharedTLSTestData* data = static_cast<SharedTLSTestData*>(arg);
    
    while (!data->ready) {
        usleep(1000); // 1ms
    }
    
    // Set different values in each ThreadLocal instance
    int value1 = data->thread_id * 10;
    int value2 = data->thread_id * 20;
    
    data->tls1->set(value1);
    data->tls2->set(value2);
    
    usleep(10000); // 10ms
    
    // Verify both values are correct
    bool success = (data->tls1->access() == value1) && (data->tls2->access() == value2);
    data->success = success;
    data->done = true;
    
    return nullptr;
}

TEST_CASE("ThreadLocal - multiple instances") {
    ThreadLocal<int> tls1;
    ThreadLocal<int> tls2;
    
    const int num_threads = 3;
    pthread_t threads[num_threads];
    SharedTLSTestData thread_data[num_threads];
    
    // Initialize thread data
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].tls1 = &tls1;
        thread_data[i].tls2 = &tls2;
        thread_data[i].thread_id = i + 1; // 1, 2, 3
    }
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        int result = pthread_create(&threads[i], nullptr, shared_tls_test_func, &thread_data[i]);
        REQUIRE(result == 0);
    }
    
    // Signal threads to start
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].ready = true;
    }
    
    // Wait for completion
    for (int i = 0; i < num_threads; i++) {
        while (!thread_data[i].done) {
            usleep(1000); // 1ms
        }
    }
    
    // Join threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], nullptr);
    }
    
    // Verify all threads succeeded
    for (int i = 0; i < num_threads; i++) {
        REQUIRE(thread_data[i].success);
    }
    
    // Main thread should have default values
    REQUIRE(tls1.access() == 0);
    REQUIRE(tls2.access() == 0);
}

TEST_CASE("ThreadLocal - copy constructor") {
    ThreadLocal<int> tls1(555);
    ThreadLocal<int> tls2(tls1);  // Copy constructor
    
    // Both should have the same default value
    REQUIRE(tls1.access() == 555);
    REQUIRE(tls2.access() == 555);
    
    // But they should be independent instances
    tls1.set(111);
    tls2.set(222);
    
    REQUIRE(tls1.access() == 111);
    REQUIRE(tls2.access() == 222);
}

TEST_CASE("ThreadLocal - assignment operator") {
    ThreadLocal<int> tls1(777);
    ThreadLocal<int> tls2;
    
    tls2 = tls1;  // Assignment operator
    
    // Both should have the same default value
    REQUIRE(tls1.access() == 777);
    REQUIRE(tls2.access() == 777);
    
    // But they should be independent instances
    tls1.set(333);
    tls2.set(444);
    
    REQUIRE(tls1.access() == 333);
    REQUIRE(tls2.access() == 444);
}

struct CleanupTestData {
    ThreadLocal<fl::string>* tls;
    volatile bool thread_started;
    volatile bool thread_finished;
    
    CleanupTestData() : tls(nullptr), thread_started(false), thread_finished(false) {}
};

static void* cleanup_test_func(void* arg) {
    CleanupTestData* data = static_cast<CleanupTestData*>(arg);
    
    data->thread_started = true;
    
    // Set a value in the thread-local storage
    data->tls->set("thread_value");
    
    // Verify the value is set
    if (data->tls->access() != "thread_value") {
        data->thread_finished = true;
        return nullptr;
    }
    
    data->thread_finished = true;
    return nullptr;
    // Thread exits here, triggering cleanup
}

TEST_CASE("ThreadLocal - thread cleanup") {
    ThreadLocal<fl::string> tls("default");
    CleanupTestData data;
    data.tls = &tls;
    
    pthread_t thread;
    int result = pthread_create(&thread, nullptr, cleanup_test_func, &data);
    REQUIRE(result == 0);
    
    // Wait for thread to start and finish
    while (!data.thread_started) {
        usleep(1000); // 1ms
    }
    
    while (!data.thread_finished) {
        usleep(1000); // 1ms
    }
    
    pthread_join(thread, nullptr);
    
    // Main thread should still have default value
    REQUIRE(tls.access() == "default");
    
    // Set a value in main thread
    tls.set("main_value");
    REQUIRE(tls.access() == "main_value");
}

TEST_CASE("ThreadLocal - const access") {
    const ThreadLocal<int> tls(888);
    
    // Should be able to access const ThreadLocal
    REQUIRE(tls.access() == 888);
    
    // Conversion operator should work with const
    int value = tls;
    REQUIRE(value == 888);
}

TEST_CASE("ThreadLocal - RAII behavior") {
    // Test that ThreadLocal properly manages its pthread_key
    {
        ThreadLocal<int> tls(123);
        REQUIRE(tls.access() == 123);
        tls.set(456);
        REQUIRE(tls.access() == 456);
    } // tls goes out of scope here, should clean up pthread_key
    
    // Create a new ThreadLocal after the previous one was destroyed
    ThreadLocal<int> tls2(789);
    REQUIRE(tls2.access() == 789);
}


#endif  // FASTLED_USE_THREAD_LOCAL
