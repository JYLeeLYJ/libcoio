OUT_DIR := $(PROJ_DIR)/bin/pingpong
SRC_DIR := $(PROJ_DIR)/example/pingpong
TMP_DIR := $(PROJ_DIR)/tmp

LINK := -lpthread -l:liburing.a

# OBJS := $(patsubst %.cpp,$(TMP_DIR)/%.o,$(notdir $(shell ls $(SRC_DIR)/*.cpp)))
OBJS := $(TMP_DIR)/client.o $(TMP_DIR)/server.o

-include $(OBJS:.o=.o.d)

$(shell if [ ! -e $(OUT_DIR) ]; then mkdir -p $(OUT_DIR) ; fi)

.PHONY : all
all: $(OBJS)
	$(CXX) $(TMP_DIR)/client.o -o $(OUT_DIR)/pingpong_client $(LINK)
	$(CXX) $(TMP_DIR)/server.o -o $(OUT_DIR)/pingpong_server $(LINK)
	cp test.sh $(OUT_DIR)

$(TMP_DIR)/%.o : $(SRC_DIR)/%.cpp
	$(CXX) $< -o $@ -c $(CXXFLAGS) -MMD -MF $@.d
