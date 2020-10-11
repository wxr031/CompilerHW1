CXXFLAGS = -std=c++17 -Wall -O3 -DNDEBUG
DEBUG = -DDEBUG

all: AcDc

AcDc: AcDc.cpp
	$(CXX) $(CXXFLAGS) AcDc.cpp -o AcDc
debug:
	$(CXX) $(CXXFLAGS) $(DEBUG) AcDc.cpp -o AcDc
clean:
	$(RM) AcDc
