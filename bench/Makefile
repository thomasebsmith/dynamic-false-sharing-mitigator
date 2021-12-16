CC = clang++ -O3 -pthread

sharedArray: sharedArray.cpp
	$(CC) $^ -o $@
sharedStruct: sharedStruct.cpp
	$(CC) $^ -o $@
basicGlobals: basicGlobals.cpp
	$(CC) $^ -o $@
locks: locks.cpp
	$(CC) $^ -o $@
basicLocks: basicLocks.cpp
	$(CC) $^ -o $@

clean:
	rm -f sharedArray sharedStruct basicGlobals locks basicLocks

.PHONY: clean

