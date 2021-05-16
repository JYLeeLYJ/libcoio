OUT_DIR := $(PROJ_DIR)/bin
SRC_DIR := $(PROJ_DIR)/test
TMP_DIR := $(PROJ_DIR)/tmp
TARGET := $(OUT_DIR)/coio_test

LINK := -lgtest -lgtest_main -lpthread -l:liburing.a -l:libcares_static.a

# OBJS := $(patsubst %.cpp,$(TMP_DIR)/%.o,$(notdir $(shell ls $(SRC_DIR)/*.cpp)))
OBJS := $(TMP_DIR)/http_test.o $(TMP_DIR)/common_test.o

-include $(OBJS:.o=.o.d)

.PHONY: test
test: $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(CXXFLAGS) $(LINK)
	cd $(PROJ_DIR) && $(TARGET)

$(TMP_DIR)/%.o : $(SRC_DIR)/%.cpp
	$(CXX) $< -o $@ -c $(CXXFLAGS) -MMD -MF $@.d
