

build:
	g++ -std=c++17 -pthread -Wl,--no-as-needed -O3 -march=native *.cpp -o test.o
