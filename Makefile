all: trace.cpp 
	g++ -Wall -Werror -pedantic -std=c++1y -pthread -o trace trace.cpp 