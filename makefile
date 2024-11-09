all:
	g++ -std=c++23 -O3 -o target/main src/*.cpp -Wno-deprecated-declarations
