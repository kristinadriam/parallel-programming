#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cctype>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

#include "md5.h"
#include "sha1.h"

static std::atomic<bool> g_shutdown{false};
static int g_socket_fd = -1;

static void signal_handler(int) {
    g_shutdown.store(true, std::memory_order_relaxed);
    int fd = g_socket_fd;
    if (fd >= 0) {
        ::shutdown(fd, SHUT_RDWR);
    }
}

static void setup_signals() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);
}

static const char CHARSET[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static const int CHARSET_SIZE = 62;
static const int MAX_KEY_LEN = 32;

static int char_index(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 36 + (c - 'a');
    return -1;
}

static bool is_valid_key(const std::string& s) {
    if (s.empty() || s.size() > MAX_KEY_LEN) return false;
    for (char c : s) {
        if (char_index(c) < 0) return false;
    }
    return true;
}

static bool is_valid_sha1_hex(const std::string& h) {
    if (h.size() != 40) return false;
    for (unsigned char uc : h) {
        if (std::isxdigit(uc) == 0) return false;
    }
    return true;
}

static std::string lowercase_hex(const std::string& h) {
    std::string out = h;
    for (char& c : out) {
        if (c >= 'A' && c <= 'F') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

static int shortlex_compare(const std::string& a, const std::string& b) {
    if (a.size() < b.size()) return -1;
    if (a.size() > b.size()) return 1;
    for (size_t i = 0; i < a.size(); i++) {
        int ai = char_index(a[i]);
        int bi = char_index(b[i]);
        if (ai < bi) return -1;
        if (ai > bi) return 1;
    }
    return 0;
}

static bool shortlex_next(std::string& s) {
    for (int i = static_cast<int>(s.size()) - 1; i >= 0; i--) {
        int idx = char_index(s[i]);
        if (idx < CHARSET_SIZE - 1) {
            s[i] = CHARSET[idx + 1];
            for (size_t j = i + 1; j < s.size(); j++) {
                s[j] = CHARSET[0];
            }
            return true;
        }
    }

    if (s.size() < static_cast<size_t>(MAX_KEY_LEN)) {
        size_t new_len = s.size() + 1;
        s.assign(new_len, CHARSET[0]);
        return true;
    }

    return false;
}


static std::string compute_hash(const std::string& key) {
    uint8_t md5_raw[16];
    MD5::hash_raw(key, md5_raw);
    return SHA1::hash_raw(md5_raw, 16);
}

struct SearchContext {
    std::string target_hash;
    std::string range_start;
    std::string range_end;

    std::atomic<bool> found{false};
    std::string found_key;
    std::mutex result_mutex;
    std::condition_variable result_cv;
    std::atomic<bool> done{false};

    std::mutex batch_mutex;
    std::string next_key;
    bool exhausted = false;
    static const uint64_t BATCH_SIZE = 50000;
};

static void worker_thread(SearchContext* ctx) {
    while (!ctx->found.load(std::memory_order_relaxed) &&
           !g_shutdown.load(std::memory_order_relaxed)) {

        std::string batch_start;
        std::string batch_end;
        bool inclusive_tail = false;
        {
            std::lock_guard<std::mutex> lock(ctx->batch_mutex);
            if (ctx->exhausted) return;

            batch_start = ctx->next_key;

            std::string advanced = ctx->next_key;
            uint64_t steps = 0;
            for (steps = 0; steps < SearchContext::BATCH_SIZE; steps++) {
                if (shortlex_compare(advanced, ctx->range_end) > 0) {
                    ctx->exhausted = true;
                    break;
                }
                if (!shortlex_next(advanced)) {
                    ctx->exhausted = true;
                    break;
                }
            }

            if (steps == 0 && ctx->exhausted) {
                if (shortlex_compare(batch_start, ctx->range_end) > 0) {
                    return;
                }
            }

            if (!ctx->exhausted) {
                batch_end = advanced;
                ctx->next_key = advanced;
            } else {
                inclusive_tail = true;
            }
        }

        std::string current = batch_start;
        while (!ctx->found.load(std::memory_order_relaxed) &&
               !g_shutdown.load(std::memory_order_relaxed)) {

            if (shortlex_compare(current, ctx->range_end) > 0) break;
            if (!inclusive_tail && shortlex_compare(current, batch_end) >= 0) break;

            std::string h = compute_hash(current);
            if (h == ctx->target_hash) {
                ctx->found.store(true, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(ctx->result_mutex);
                ctx->found_key = current;
                ctx->done.store(true);
                ctx->result_cv.notify_all();
                return;
            }

            if (!shortlex_next(current)) break;
        }
    }
}

static bool parallel_search(const std::string& target_hash,
                            const std::string& range_start,
                            const std::string& range_end,
                            std::string& result_key) {
    SearchContext ctx;
    ctx.target_hash = target_hash;
    ctx.range_start = range_start;
    ctx.range_end = range_end;
    ctx.next_key = range_start;

    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    std::cerr << "[INFO] Starting search with " << num_threads << " threads" << std::endl;
    std::cerr << "[INFO] Range: [" << range_start << " ; " << range_end << "]" << std::endl;

    for (unsigned int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker_thread, &ctx);
    }

    for (auto& t : threads) {
        t.join();
    }

    if (ctx.found.load()) {
        result_key = ctx.found_key;
        return true;
    }
    return false;
}

static int connect_to_server(const char* host, int port) {
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    int err = getaddrinfo(host, port_str.c_str(), &hints, &res);
    if (err != 0) {
        std::cerr << "[ERROR] getaddrinfo: " << gai_strerror(err) << std::endl;
        return -1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        std::cerr << "[ERROR] socket: " << strerror(errno) << std::endl;
        freeaddrinfo(res);
        return -1;
    }

    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        std::cerr << "[ERROR] connect: " << strerror(errno) << std::endl;
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return fd;
}

static bool send_line(int fd, const std::string& msg) {
    std::string line = msg + "\n";
    const char* data = line.c_str();
    size_t remaining = line.size();

    while (remaining > 0) {
        ssize_t sent = send(fd, data, remaining, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) continue;
            std::cerr << "[ERROR] send: " << strerror(errno) << std::endl;
            return false;
        }
        data += sent;
        remaining -= sent;
    }
    return true;
}

static bool recv_line(int fd, std::string& line) {
    line.clear();
    char c;
    while (true) {
        ssize_t r = recv(fd, &c, 1, 0);
        if (r < 0) {
            if (errno == EINTR) {
                if (g_shutdown.load()) return false;
                continue;
            }
            std::cerr << "[ERROR] recv: " << strerror(errno) << std::endl;
            return false;
        }
        if (r == 0) {
            if (!line.empty()) return true;
            return false;
        }
        if (c == '\n') return true;
        if (c != '\r') line += c;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <host> <port> <hashrate_file>" << std::endl;
        return 1;
    }

    const char* host = argv[1];

    int port = 0;
    try {
        port = std::stoi(argv[2]);
    } catch (...) {
        std::cerr << "[ERROR] Invalid port: " << argv[2] << std::endl;
        return 1;
    }
    if (port < 1 || port > 65535) {
        std::cerr << "[ERROR] Port out of range: " << port << std::endl;
        return 1;
    }

    std::string hashrate_str;
    {
        std::ifstream file(argv[3]);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Cannot open hashrate file: " << argv[3] << std::endl;
            return 1;
        }
        if (!(file >> hashrate_str)) {
            std::cerr << "[ERROR] Cannot read hashrate from file" << std::endl;
            return 1;
        }
    }

    long long hashrate = 0;
    try {
        hashrate = std::stoll(hashrate_str);
    } catch (...) {
        std::cerr << "[ERROR] Invalid hashrate value: " << hashrate_str << std::endl;
        return 1;
    }
    if (hashrate <= 0) {
        std::cerr << "[ERROR] Hashrate must be positive" << std::endl;
        return 1;
    }

    setup_signals();

    std::cerr << "[INFO] Connecting to " << host << ":" << port << std::endl;
    g_socket_fd = connect_to_server(host, port);
    if (g_socket_fd < 0) {
        return 1;
    }
    std::cerr << "[INFO] Connected" << std::endl;

    if (!send_line(g_socket_fd, "HASHRATE " + hashrate_str)) {
        close(g_socket_fd);
        return 1;
    }

    while (!g_shutdown.load()) {
        std::string line;
        if (!recv_line(g_socket_fd, line)) {
            if (g_shutdown.load()) {
                std::cerr << "[INFO] Shutting down by signal" << std::endl;
            } else {
                std::cerr << "[ERROR] Lost connection to server" << std::endl;
            }
            break;
        }

        std::cerr << "[INFO] Server: " << line << std::endl;

        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "TASK") {
            std::string hash, range_start, range_end;
            iss >> hash >> range_start >> range_end;

            if (hash.empty() || range_start.empty() || range_end.empty()) {
                std::cerr << "[ERROR] Malformed TASK message" << std::endl;
                send_line(g_socket_fd, "ERROR malformed_task");
                continue;
            }

            if (!is_valid_sha1_hex(hash)) {
                std::cerr << "[ERROR] Invalid TASK hash (expect 40 hex digits)" << std::endl;
                send_line(g_socket_fd, "ERROR invalid_hash");
                continue;
            }
            const std::string hash_norm = lowercase_hex(hash);

            if (!is_valid_key(range_start) || !is_valid_key(range_end)) {
                std::cerr << "[ERROR] Invalid range characters" << std::endl;
                send_line(g_socket_fd, "ERROR invalid_range");
                continue;
            }

            if (shortlex_compare(range_start, range_end) > 0) {
                std::cerr << "[ERROR] Invalid range: start > end" << std::endl;
                send_line(g_socket_fd, "ERROR invalid_range");
                continue;
            }

            std::string found_key;
            bool found = parallel_search(hash_norm, range_start, range_end, found_key);

            if (g_shutdown.load()) {
                std::cerr << "[INFO] Search interrupted by signal" << std::endl;
                break;
            }

            if (found) {
                std::cerr << "[INFO] Key found: " << found_key << std::endl;
                if (!send_line(g_socket_fd, "FOUND " + found_key)) break;
            } else {
                std::cerr << "[INFO] Key not found in range" << std::endl;
                if (!send_line(g_socket_fd, "NOT_FOUND")) break;
            }

        } else if (command == "WAIT") {
            int seconds = 0;
            iss >> seconds;
            if (seconds <= 0) seconds = 1;

            std::cerr << "[INFO] Waiting " << seconds << " seconds" << std::endl;

            for (int i = 0; i < seconds && !g_shutdown.load(); i++) {
                sleep(1);
            }

            if (!g_shutdown.load()) {
                if (!send_line(g_socket_fd, "HASHRATE " + hashrate_str)) break;
            }

        } else if (command == "DONE") {
            std::cerr << "[INFO] Server sent DONE, exiting" << std::endl;
            break;

        } else {
            if (command.empty()) {
                std::cerr << "[WARN] Empty line from server" << std::endl;
                if (!send_line(g_socket_fd, "ERROR empty_message")) break;
            } else {
                std::cerr << "[WARN] Unknown command: " << command << std::endl;
                if (!send_line(g_socket_fd, "ERROR unknown_command")) break;
            }
        }
    }

    if (g_socket_fd >= 0) {
        close(g_socket_fd);
        g_socket_fd = -1;
    }

    std::cerr << "[INFO] Client terminated" << std::endl;
    return 0;
}
