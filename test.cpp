#include <chrono>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <ctime>
#include <unordered_map>
#include <thread>
#include <pthread.h>
#include <cstdlib>
#include <functional>

const uint32_t size = 1000000000; // 1B rows
const uint32_t num_threads = 96;

std::vector<std::string> strCol;
std::vector<std::string> strCol2;
std::vector<double> doubleCol;
std::vector<double> doubleCol2;
std::unordered_map<std::string, double> hashMap;

std::vector<double> doubleResultCol(size, 0);

std::string gen_random(const int len) {
    
    std::string tmp_s;
    static const char alphanum[] = "0123456789";
    
    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) 
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    
    
    return tmp_s;
    
}

void floatMapper(int start, int end) {
    for (int i = start; i < end; i++) {
        doubleResultCol[i] = doubleCol[i] * 2.5;
    }
}

void hashBuild(int start, int end) {
    for (int i = start; i < end; i++) {
        if (hashMap.find(strCol.at(i)) == hashMap.end()) {
            hashMap[strCol.at(i)] = 0;
        }
        hashMap[strCol.at(i)] += doubleCol.at(i);
    }
}

void hashProbe(int start, int end) {
    for (int i = start; i < end; i++) {
        if (hashMap.find(strCol2.at(i)) == hashMap.end()) {
            hashMap[strCol2.at(i)] = 0.0;
        }
        hashMap[strCol2.at(i)] += doubleCol2.at(i);
    }
}

void genData() {
    hashMap.reserve(size);
    for (int i = 0; i < size; i++) {
        strCol.push_back(gen_random(4));
        strCol2.push_back(gen_random(4));
        doubleCol.push_back(rand());
        doubleCol2.push_back(rand());
    }
}

struct benchmarkArg {
    int start;
    int end;
    std::function<void(int, int)> func;
};

void* benchmarkHelper(void* arg) {
    struct benchmarkArg* castedArg = (struct benchmarkArg*) arg;
    int start = castedArg->start;
    int end = castedArg->end;

    castedArg->func(start, end);
}

void hashHelper(int start, int end) {
    hashBuild(start, end);
    hashProbe(start, end);
}

void benchmarkConcurrent() {
    pthread_t hashThreads[num_threads / 2];
    pthread_t doubleThreads[num_threads / 2];

    int segmentSize = size / (num_threads / 2);

    for (int i = 0; i < num_threads / 2; i++) {
        struct benchmarkArg* arg = (struct benchmarkArg*) malloc(sizeof(struct benchmarkArg));
        struct benchmarkArg* arg2 = (struct benchmarkArg*) malloc(sizeof(struct benchmarkArg));

        int start = segmentSize * i;
        int end = segmentSize * (i + 1);

        arg->start = start;
        arg->end = end;
        arg->func = hashHelper;

        arg2->start = start;
        arg2->end = end;
        arg2->func = floatMapper;

        pthread_create(&hashThreads[i], NULL, benchmarkHelper, arg);
        pthread_create(&doubleThreads[i], NULL, benchmarkHelper, arg);
        
        cpu_set_t* cpuset = (cpu_set_t*) malloc(sizeof(cpu_set_t));
        CPU_ZERO(cpuset);
        CPU_SET(i, cpuset);
        if (pthread_setaffinity_np(hashThreads[i], sizeof(cpu_set_t), cpuset)) {
            std::printf("Hash affinity setting on thread %d failed.\n", i);
            exit(1);
        }

        cpu_set_t* cpuset2 = (cpu_set_t*) malloc(sizeof(cpu_set_t));
        CPU_ZERO(cpuset2);
        CPU_SET(i + (num_threads / 2), cpuset2);
        if (pthread_setaffinity_np(doubleThreads[i], sizeof(cpu_set_t), cpuset2)) {
            std::printf("Hash affinity setting on thread %d failed.\n", i + (num_threads / 2));
            exit(1);
        }
    }

    for (int i = 0; i < num_threads / 2; i++) {
        pthread_join(hashThreads[i], NULL);
        pthread_join(doubleThreads[i], NULL);
    }
}

void benchmarkSequential() {
    std::vector<std::thread> hashThreads(num_threads);
    std::vector<std::thread> doubleThreads(num_threads);

    int segmentSize = size / num_threads;

    for (int i = 0; i < num_threads; i++) {
        hashThreads[i] = std::thread([=] {
            int start = segmentSize * i;
            int end = segmentSize * (i + 1);

            hashBuild(start, end);
            hashProbe(start, end);
        });
    }
    for (std::thread& t : hashThreads) {
        t.join();
    }
    for (int i = 0; i < num_threads; i++) {
        doubleThreads[i] = std::thread([=] {
            int start = segmentSize * i;
            int end = segmentSize * (i + 1);

            floatMapper(start, end);
        });
    }
    for (std::thread& t : doubleThreads) {
        t.join();
    }
}

void benchmarkNaiveConcurrent() {
    std::vector<std::thread> hashThreads(num_threads);
    std::vector<std::thread> doubleThreads(num_threads);

    int segmentSize = size / num_threads;

    for (int i = 0; i < num_threads / 2; i++) {
        hashThreads[i] = std::thread([=] {
            int start = segmentSize * i;
            int end = segmentSize * (i + 1);

            hashBuild(start, end);
            hashProbe(start, end);
        });
    }
    for (int i = 0; i < num_threads / 2; i++) {
        doubleThreads[i] = std::thread([=] {
            int start = segmentSize * i;
            int end = segmentSize * (i + 1);

            floatMapper(start, end);
        });
    }
    for (std::thread& t : hashThreads) {
        t.join();
    }
    for (std::thread& t : doubleThreads) {
        t.join();
    }
}

void printEndBenchmark(std::chrono::time_point<std::chrono::high_resolution_clock> startTime, std::string msg) {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime).count();

    std::printf("%s took %lld ms\n", msg, duration);
}

int main() {
    srand(42);

    genData();

    hashMap.clear();
    hashMap.reserve(size);
    auto start = std::chrono::high_resolution_clock::now();
    benchmarkNaiveConcurrent();
    printEndBenchmark(start, "Naive concurrent");

    hashMap.clear();
    hashMap.reserve(size);
    start = std::chrono::high_resolution_clock::now();
    benchmarkSequential();
    printEndBenchmark(start, "Sequential");

    hashMap.clear();
    hashMap.reserve(size);
    start = std::chrono::high_resolution_clock::now();
    benchmarkConcurrent();
    printEndBenchmark(start, "Manual scheduled concurrent");

    std::cout << std::endl;
    std::cout << hashMap.load_factor() << " " << doubleResultCol.at(size - 1) << std::endl;
}