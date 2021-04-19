#include <chrono>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <ctime>
#include <tbb/concurrent_unordered_map.h>
#include <thread>
#include <pthread.h>
#include <cstdlib>
#include <functional>

const uint32_t size = 100000000;
const uint32_t floatMult = 17; // use more floats since they're fast
const uint32_t num_threads = 96;

std::vector<std::string> strCol;
std::vector<std::string> strCol2;
std::vector<double> doubleCol;
std::vector<double> doubleCol2;
tbb::concurrent_unordered_map<std::string, double> hashMap;

std::vector<double> doubleResultCol(size * floatMult, 0);

std::string gen_random(const int len) {
    
    std::string tmp_s;
    static const char alphanum[] = "0123456789";
    
    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) 
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    
    
    return tmp_s;
    
}

void floatMapper(int start, int end) {
	
    for (int i = start * floatMult; i < end * floatMult; i++) {
	    for (int j = 0; j < floatMult; j++) {
        doubleResultCol[i] = doubleCol[i] * doubleCol2[i] + doubleResultCol[i];
	    }
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
    for (int i = 0; i < size; i++) {
        strCol.push_back(gen_random(6));
        strCol2.push_back(gen_random(6));
        for (int j = 0; j < floatMult; j++) {
            doubleCol.push_back(rand());
            doubleCol2.push_back(rand());
        }
    }
}

void hashHelper(int start, int end) {
    hashBuild(start, end);
    hashProbe(start, end);
}

struct benchmarkArg {
    int start;
    int end;
    bool isFloat;
};

void* benchmarkHelper(void* arg) {
    struct benchmarkArg* castedArg = (struct benchmarkArg*) arg;
    int start = castedArg->start;
    int end = castedArg->end;

    if (castedArg->isFloat) {
        floatMapper(start, end);
    } else {
        hashHelper(start, end);
    }
}


void benchmarkConcurrent() {
    pthread_t hashThreads[num_threads / 8];
    pthread_t doubleThreads[num_threads / 8];

    int segmentSize = size / (num_threads / 8);

    for (int i = 0; i < num_threads / 8; i++) {
        struct benchmarkArg* arg = (struct benchmarkArg*) malloc(sizeof(struct benchmarkArg));
        struct benchmarkArg* arg2 = (struct benchmarkArg*) malloc(sizeof(struct benchmarkArg));

        int start = segmentSize * i;
        int end = segmentSize * (i + 1);

        arg->start = start;
        arg->end = end;
        arg->isFloat = false;

        arg2->start = start;
        arg2->end = end;
        arg2->isFloat = true;

        pthread_create(&hashThreads[i], NULL, benchmarkHelper, arg);
        pthread_create(&doubleThreads[i], NULL, benchmarkHelper, arg2);
        
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

    for (int i = 0; i < num_threads / 8; i++) {
        pthread_join(hashThreads[i], NULL);
        pthread_join(doubleThreads[i], NULL);
    }
}

void benchmarkSequential() {
    pthread_t hashThreads[num_threads / 4];
    pthread_t doubleThreads[num_threads / 4];

    int segmentSize = size / (num_threads / 4);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_threads / 4; i++) {
        struct benchmarkArg* arg = (struct benchmarkArg*) malloc(sizeof(struct benchmarkArg));

        int start = segmentSize * i;
        int end = segmentSize * (i + 1);

        arg->start = start;
        arg->end = end;
        arg->isFloat = false;

        pthread_create(&hashThreads[i], NULL, benchmarkHelper, arg);
        
        cpu_set_t* cpuset = (cpu_set_t*) malloc(sizeof(cpu_set_t));
        CPU_ZERO(cpuset);
        for (int j = 0; j < num_threads / 8; j++) {
            CPU_SET(j, cpuset);
            CPU_SET(j + (num_threads / 2), cpuset);
        }
        if (pthread_setaffinity_np(hashThreads[i], sizeof(cpu_set_t), cpuset)) {
            std::printf("Affinity setting on thread %d failed.\n", i);
            exit(1);
        }
    }

    for (int i = 0; i < num_threads / 4; i++) {
        pthread_join(hashThreads[i], NULL);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::printf("%lld ms\n", duration);

    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_threads / 4; i++) {
        struct benchmarkArg* arg = (struct benchmarkArg*) malloc(sizeof(struct benchmarkArg));

        int start = segmentSize * i;
        int end = segmentSize * (i + 1);

        arg->start = start;
        arg->end = end;
        arg->isFloat = true;

        pthread_create(&doubleThreads[i], NULL, benchmarkHelper, arg);
        
        cpu_set_t* cpuset = (cpu_set_t*) malloc(sizeof(cpu_set_t));
        CPU_ZERO(cpuset);
        for (int j = 0; j < num_threads / 8; j++) {
            CPU_SET(j, cpuset);
            CPU_SET(j + (num_threads / 2), cpuset);
        }
        if (pthread_setaffinity_np(doubleThreads[i], sizeof(cpu_set_t), cpuset)) {
            std::printf("Affinity setting on thread %d failed.\n", i);
            exit(1);
        }
    }

    for (int i = 0; i < num_threads / 4; i++) {
        pthread_join(doubleThreads[i], NULL);
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::printf("%lld ms\n", duration);
}

void benchmarkNaiveConcurrent() {
    pthread_t hashThreads[num_threads / 8];
    pthread_t doubleThreads[num_threads / 8];

    int segmentSize = size / (num_threads / 8);

    for (int i = 0; i < num_threads / 8; i++) {
        struct benchmarkArg* arg = (struct benchmarkArg*) malloc(sizeof(struct benchmarkArg));
        struct benchmarkArg* arg2 = (struct benchmarkArg*) malloc(sizeof(struct benchmarkArg));

        int start = segmentSize * i;
        int end = segmentSize * (i + 1);

        arg->start = start;
        arg->end = end;
        arg->isFloat = false;

        arg2->start = start;
        arg2->end = end;
        arg2->isFloat = true;

        pthread_create(&hashThreads[i], NULL, benchmarkHelper, arg);
        pthread_create(&doubleThreads[i], NULL, benchmarkHelper, arg2);
        
        cpu_set_t* cpuset = (cpu_set_t*) malloc(sizeof(cpu_set_t));
        CPU_ZERO(cpuset);
        for (int j = 0; j < num_threads / 8; j++) {
            CPU_SET(j, cpuset);
            CPU_SET(j + (num_threads / 2), cpuset);
        }
        if (pthread_setaffinity_np(hashThreads[i], sizeof(cpu_set_t), cpuset)) {
            std::printf("Hash affinity setting on thread %d failed.\n", i);
            exit(1);
        }

        cpu_set_t* cpuset2 = (cpu_set_t*) malloc(sizeof(cpu_set_t));
        CPU_ZERO(cpuset2);
        for (int j = 0; j < num_threads / 8; j++) {
            CPU_SET(j, cpuset2);
            CPU_SET(j + (num_threads / 2), cpuset2);
        }
        if (pthread_setaffinity_np(doubleThreads[i], sizeof(cpu_set_t), cpuset2)) {
            std::printf("Hash affinity setting on thread %d failed.\n", i + (num_threads / 2));
            exit(1);
        }
    }

    for (int i = 0; i < num_threads / 8; i++) {
        pthread_join(hashThreads[i], NULL);
        pthread_join(doubleThreads[i], NULL);
    }
}

void printEndBenchmark(std::chrono::time_point<std::chrono::high_resolution_clock> startTime, const char* msg) {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime).count();

    std::printf("%s took %lld ms\n", msg, duration);
}

int main() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    srand(42);

    genData();

    hashMap.clear();
    auto start = std::chrono::high_resolution_clock::now();
    benchmarkNaiveConcurrent();
    printEndBenchmark(start, "Naive concurrent");

    hashMap.clear();
    start = std::chrono::high_resolution_clock::now();
    benchmarkSequential();
    printEndBenchmark(start, "Sequential");

    hashMap.clear();
    start = std::chrono::high_resolution_clock::now();
    benchmarkConcurrent();
    printEndBenchmark(start, "Manual scheduled concurrent");

    std::cout << std::endl;
    std::cout << hashMap.load_factor() << " " << doubleResultCol.at(size - 1) << std::endl;
}
