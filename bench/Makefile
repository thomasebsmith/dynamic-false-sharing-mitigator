CC = g++ -O0 -pthread

sharedArray: sharedArray.cpp
	$(CC) $^ -o $@
sharedStruct: sharedStruct.cpp
	$(CC) $^ -o $@
basicGlobals: basicGlobals.cpp
	$(CC) $^ -o $@

clean:
	rm -f sharedArray sharedStruct basicGlobals

.PHONY: clean

