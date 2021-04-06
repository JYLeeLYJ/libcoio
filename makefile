CXX := g++
CPPSTD := c++20
OPT := -O3
INCLUDE := ./include
DEFINE := 
CXXFLAGS := $(OPT) -std=$(CPPSTD) $(DEFINE) -I$(INCLUDE) -Wall -fcoroutines 

.PHONY: test clean
clean : 
	rm -rf bin/*
	rm -rf tmp/*.o
	rm -rf tmp/*.d

test:
	make -C ./test PROJ_DIR=$(CURDIR) test 
