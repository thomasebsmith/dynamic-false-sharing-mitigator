all: MapAddr.o

MapAddr.o: MapAddr.cpp AccessInfo.cpp
	g++ MapAddr.cpp AccessInfo.cpp ../detect/InterferenceDetector.cpp -g3 -std=c++17 -o MapAddr

clean:
	rm -f MapAddr 

.PHONY: clean 
