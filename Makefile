

build:
	gcc10-c++ -o test.o *.cpp -std=c++17 -pthread -ldl -Wl,--no-as-needed -O3 -g -shared-libgcc -march=native
