#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

struct {
    std::mutex mtx;
    std::condition_variable cv;
} sync;

struct {
    int h{};
    int o{};
} atom_count;

void hydrogen() {
    std::unique_lock lk(sync.mtx);
    sync.cv.wait(lk, [] { return atom_count.h < 2; });

    std::cout << "H";

    ++atom_count.h;

    if (atom_count.h == 2 && atom_count.o == 1) {
        atom_count.h = 0;
        atom_count.o = 0;
    }

    sync.cv.notify_all();
}

void oxygen() {
    std::unique_lock lk(sync.mtx);
    sync.cv.wait(lk, [] { return atom_count.o < 1; });

    std::cout << "O";

    ++atom_count.o;

    if (atom_count.h == 2 && atom_count.o == 1) {
        atom_count.h = 0;
        atom_count.o = 0;
    }

    sync.cv.notify_all();
}

int main() {
    std::thread t1([&] { hydrogen(); });
    std::thread t2([&] { hydrogen(); });
    std::thread t3([&] { oxygen(); });
    std::thread t4([&] { hydrogen(); });
    std::thread t5([&] { oxygen(); });
    std::thread t6([&] { hydrogen(); });

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    t6.join();

    std::cout << std::endl;
}