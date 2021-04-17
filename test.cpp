#include <chrono>
#include <iostream>
#include <vector>
#include <unistd.h>
#include <ctime>
#include <unordered_map>
#include <thread>

const uint32_t size = 10000000; // 10M rows

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

void benchmarkConcurrent() {
    std::vector<std::thread> hashThreads(6);
    std::vector<std::thread> doubleThreads(6);

    int segmentSize = size / 6;

    for (int i = 0; i < 6; i++) {
        hashThreads[i] = std::thread([=] {
            int start = segmentSize * i;
            int end = segmentSize * (i + 1);

            hashBuild(start, end);
            hashProbe(start, end);
        });

        doubleThreads[i] = std::thread([=] {
            int start = segmentSize * i;
            int end = segmentSize * (i + 1);

            hashBuild(start, end);
            hashProbe(start, end);
        });
    }
}

void benchmarkSequential() {
    std::vector<std::thread> hashThreads(12);
    std::vector<std::thread> doubleThreads(12);

    int segmentSize = size / 12;

    for (int i = 0; i < 12; i++) {
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
    for (int i = 0; i < 12; i++) {
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

}

int main() {
    srand(42);

    genData();

    std::cout << hashMap.load_factor() << " " << doubleResultCol.at(size - 1) << std::endl;
}