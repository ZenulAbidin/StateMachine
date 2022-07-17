# This makefile only supports C++ files.
# If you want to mix compile C and C++,
# then you must either add/change the corresponding
# names in the makefile, or change the C file
# extensions to C++ file extensions.
# It's excessively compilcated to create duplicate
# rules for each file type anyway, and if you find
# yourself in a situaion where you must compile
# C and C++ together, you should consider using
# automake instead. It handles these kind of things
# better than a lone makefile ever could.
CC = g++
CXXFLAGS = -fPIC -O3 
INCLUDE_FILES = -I./include
LDLIBS = -lpthread
LDFLAGS = -shared

SRC_DIR = src
EXAMPLES_DIR = examples
EXAMPLES_BIN = StateMachineExamples
EXAMPLES_DEPLIBS = -L. libStateMachine.so
LIB_FILES = libStateMachine.so
BIN_FILES = $(EXAMPLES_BIN) 


SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
EXAMPLES_FILES := $(wildcard $(EXAMPLES_DIR)/*.cpp)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp, $(SRC_DIR)/%.o, $(SRC_FILES))
OBJ_EXAMPLES_FILES := $(patsubst $(EXAMPLES_DIR)/%.cpp, $(EXAMPLES_DIR)/%.o, $(EXAMPLES_FILES))


all: ${LIB_FILES} ${BIN_FILES}

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp 
	$(CC) $(INCLUDE_FILES) $(CXXFLAGS) -c $< -o $@

$(EXAMPLES_DIR)/%.o: $(EXAMPLES_DIR)/%.cpp
	$(CC) $(INCLUDE_FILES) $(CXXFLAGS) -c $< -o $@


${LIB_FILES} : ${OBJ_FILES}
	${CC} -o $@ $(INCLUDE_FILES) ${LDFLAGS} $^ ${LDLIBS}

${EXAMPLES_BIN} : ${OBJ_EXAMPLES_FILES}
	${CC} -o $@ $(INCLUDE_FILES) ${BINFLAGS} $^ ${LDLIBS} ${EXAMPLES_DEPLIBS}

.PHONY: clean

clean:
	rm -f ${SRC_DIR}/*.o ${EXAMPLES_DIR}/*.o *.so ${BIN_FILES}
