CPPFLAGS=-pthread -g -std=c++11 -Wall

threadWar: threadWar.cpp
	g++ -o threadWar threadWar.cpp $(CPPFLAGS)

.PHONY: clean

clean:
	rm -f threadWar
	rm -f log.txt
