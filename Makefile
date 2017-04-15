all: trace.cpp 
	g++ -Wall -pedantic -std=c++1y -pthread -o trace trace.cpp 