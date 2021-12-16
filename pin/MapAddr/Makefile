all: MapAddr.o

MapAddr.o: MapAddr.cpp 
	g++ MapAddr.cpp ../detect/InterferenceDetector.cpp -g3 -std=c++17 -o MapAddr

clean:
	rm -f MapAddr 

.PHONY: clean 
