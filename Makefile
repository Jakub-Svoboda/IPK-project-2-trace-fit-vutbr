all: trace.cpp 
	g++ -Wall -std=c++1y -pthread -o trace trace.cpp 