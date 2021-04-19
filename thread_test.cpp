#include <pthread.h>
#include <cstdlib>
#include <chrono>
#include <iostream>

int global_int = 0;

void* tester(void* arg) {
	global_int += ((char*) arg)[0];
}

int main() {
	auto start = std::chrono::high_resolution_clock::now();
	pthread_t threads[512];
	for (int i = 0; i < 512; i++) {
		char* buf = (char*) malloc(512);	
		pthread_create(&threads[i], NULL, tester, buf);
	}

	for (int i = 0; i < 512; i++) {
		pthread_join(threads[i], NULL);
	}
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	std::printf("%lld ms\n", duration);
}
