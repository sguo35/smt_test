

build:
	g++ -std=c++17 -lpthread -O3 -mavx2 -march=native *.cpp -o test.o