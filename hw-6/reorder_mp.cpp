#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>

#if defined(__APPLE__)
#include <pthread.h>

static void tune_thread() {
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
}
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>

static void tune_thread_pin(int cpu) {
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    pthread_setaffinity_np(pthread_self(), sizeof(s), &s);
}
#endif

#if defined(FLAG_ATOMIC_ACQREL)
alignas(64) std::atomic<int> g_data{0};
alignas(64) std::atomic<int> g_ready{0};
#define DATA_STORE(v) g_data.store((v), std::memory_order_relaxed)
#define READY_STORE(v) g_ready.store((v), std::memory_order_release)
#define DATA_LOAD() g_data.load(std::memory_order_relaxed)
#define READY_LOAD() g_ready.load(std::memory_order_acquire)
static const char *MODE = "std::atomic<int>, release/acquire (корректно)";
#elif defined(FLAG_ATOMIC_RELAXED)
alignas(64) std::atomic<int> g_data{0};
alignas(64) std::atomic<int> g_ready{0};
#define DATA_STORE(v) g_data.store((v), std::memory_order_relaxed)
#define READY_STORE(v) g_ready.store((v), std::memory_order_relaxed)
#define DATA_LOAD() g_data.load(std::memory_order_relaxed)
#define READY_LOAD() g_ready.load(std::memory_order_relaxed)
static const char *MODE = "std::atomic<int>, memory_order_relaxed";
#else  // FLAG_PLAIN
alignas(64) volatile int g_data = 0;
alignas(64) volatile int g_ready = 0;
#define DATA_STORE(v) g_data = (v)
#define READY_STORE(v) g_ready = (v)
#define DATA_LOAD() g_data
#define READY_LOAD() g_ready
static const char *MODE = "обычный volatile int — data race, UB";
#endif

static std::atomic<bool> g_go{false};
static std::atomic<long> g_anom{0};
static long ITERS = 100000000;

static void writer() {
#if defined(__APPLE__)
    tune_thread();
#elif defined(__linux__)
    tune_thread_pin(0);
#endif
    while (!g_go.load(std::memory_order_acquire)) {
    }
    for (long i = 1; i <= ITERS; ++i) {
        DATA_STORE((int)i);
        READY_STORE((int)i);
    }
}

static void reader() {
#if defined(__APPLE__)
    tune_thread();
#elif defined(__linux__)
    tune_thread_pin(1);
#endif
    while (!g_go.load(std::memory_order_acquire)) {
    }

    int last = 0;
    long anom = 0;
    while (last < (int)ITERS) {
        int r = READY_LOAD();
        if (r != last) {
            int d = DATA_LOAD();
            if (d < r) {
                ++anom;
            }
            last = r;
        }
    }
    g_anom.store(anom, std::memory_order_release);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        ITERS = atol(argv[1]);
    }

    std::thread tw(writer), tr(reader);
    g_go.store(true, std::memory_order_release);

    tw.join();
    tr.join();

    printf("Режим:   %s\n", MODE);
    printf(
        "Итераций: %ld, аномалий (data < ready): %ld\n", ITERS, g_anom.load()
    );

    return 0;
}