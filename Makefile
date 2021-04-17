

build:
	g++ -std=c++17 -pthread -lpthread -O3 -mavx2 -march=native *.cpp -o test.o