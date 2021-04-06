CXX := g++
CPPSTD := -std=c++20
OPT := -O3 -fcoroutines -fconcepts-diagnostics-depth=5 #-ftime-report 
INCLUDE := $(PROJ_DIR)/include
DEFINE := 
WARNING := -Wall 
LINK := -lgtest -lgtest_main -lpthread
CXXFLAGS := $(OPT) $(CPPSTD) $(DEFINE) -I$(INCLUDE) $(WARNING)

OUT_DIR := $(PROJ_DIR)/bin
SRC_DIR := $(PROJ_DIR)/test
TMP_DIR := $(PROJ_DIR)/tmp
TARGET := $(OUT_DIR)/coio_test

$(shell if [ ! -e $(OUT_DIR) ]; then mkdir -p $(OUT_DIR) ; fi)
$(shell if [ ! -e $(TMP_DIR) ]; then mkdir -p $(TMP_DIR) ; fi)

OBJS := $(patsubst %.cpp,$(TMP_DIR)/%.o,$(notdir $(shell ls $(SRC_DIR)/*.cpp)))

-include $(OBJS:.o=.o.d)

.PHONY: test
test: $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(CXXFLAGS) $(LINK)
	$(TARGET)

$(TMP_DIR)/%.o : $(SRC_DIR)/%.cpp
	$(CXX) $< -o $@ -c $(CXXFLAGS) -MMD -MF $@.d
