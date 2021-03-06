
CPPSTD := -std=c++20
OPT := -O3 -fcoroutines -g #-ftime-report 
INCLUDE := $(CURDIR)/include
DEFINE := 
WARNING := -Wall -Werror

export CXX := g++
export CXXFLAGS := $(OPT) $(CPPSTD) $(DEFINE) -I$(INCLUDE) $(WARNING)
export PROJ_DIR := $(CURDIR)

OUT_DIR := $(CURDIR)/bin
TMP_DIR := $(CURDIR)/tmp

$(shell if [ ! -e $(OUT_DIR) ]; then mkdir -p $(OUT_DIR) ; fi)
$(shell if [ ! -e $(TMP_DIR) ]; then mkdir -p $(TMP_DIR) ; fi)

.PHONY: test example
clean : 
	rm -rf bin/*
	rm -rf tmp/*.o
	rm -rf tmp/*.d

test:
	+make -C ./test test 

example :
	+make -C ./example 

format :
	find include -name "*.hpp" |xargs clang-format -i 
	find example -name "*.cpp" |xargs clang-format -i
	find test -name "*.cpp" |xargs clang-format -i
