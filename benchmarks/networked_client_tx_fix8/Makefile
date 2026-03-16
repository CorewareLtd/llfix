#Compiler
CXX=g++
#Source Directories
SOURCE_DIR=.
SOURCES = $(SOURCE_DIR)/benchmark.cpp \
	$(SOURCE_DIR)/generated_code/Myfix_classes.cpp \
	$(SOURCE_DIR)/generated_code/Myfix_traits.cpp \
	$(SOURCE_DIR)/generated_code/Myfix_types.cpp
#Include Directories
FIX8_PREFIX ?= /usr/local
INCLUDE_DIRS = -I../../include/ -I$(FIX8_PREFIX)/include/ -I$(SOURCE_DIR)/generated_code
#Objects
OBJECTS = $(SOURCES:.cpp=.o)
#Executable
EXECUTABLE = ./benchmark
#Compiler flags
CFLAGS= $(INCLUDE_DIRS) -std=c++17 -c
#Linker flags
LFLAGS= -L$(FIX8_PREFIX)/lib -L$(FIX8_PREFIX)/lib64 -lfix8 -lPocoNet -lPocoFoundation -lPocoUtil -lstdc++ -pthread

#Add DEBUG macro , symbol generation and show all warnings
debug: CFLAGS += -DDEBUG -g -Wall -Wextra -Wpedantic -fno-omit-frame-pointer
debug: all
#unresolved-symbols=ignore-in-shared-libs is for sanitizers
#as sanitizers cause additional code to be added
#Debug mode + compile and link with GCC address sanitizer 
debug_with_asan: CFLAGS += -DDEBUG -g -Wall -fno-omit-frame-pointer
debug_with_asan: LFLAGS += -Wl,-z,defs -fsanitize=address -unresolved-symbols=ignore-in-shared-libs
debug_with_asan: all
#Debug mode + compile and link with GCC leak sanitizer
debug_with_lsan: CFLAGS += -DDEBUG -g -Wall -fno-omit-frame-pointer
debug_with_lsan: LFLAGS += -Wl,-z,defs -fsanitize=leak -unresolved-symbols=ignore-in-shared-libs
debug_with_lsan: all
#Debug mode + compile and link with GCC thread sanitizer 
debug_with_tsan: CFLAGS += -DDEBUG -g -Wall -fno-omit-frame-pointer
debug_with_tsan: LFLAGS += -Wl,-z,defs -fsanitize=thread -unresolved-symbols=ignore-in-shared-libs
debug_with_tsan: all
#Debug mode + compile and link with GCC undefined behaviour sanitizer 
debug_with_ubsan: CFLAGS += -DDEBUG -g -Wall -fno-omit-frame-pointer
debug_with_ubsan: LFLAGS += -Wl,-z,defs -fsanitize=undefined -unresolved-symbols=ignore-in-shared-libs
debug_with_ubsan: all

#Release mode
release: CFLAGS += -DNDEBUG -O3 -march=native -mtune=native
release: all
all: $(OBJECTS) $(EXECUTABLE)

$(EXECUTABLE) : $(OBJECTS)
		$(CXX) $(OBJECTS) $(LFLAGS) -o $@ 
	
.cpp.o: *.h
	$(CXX) $(CFLAGS) $< -o $@

clean:
	@echo Cleaning
	-rm -f $(OBJECTS) $(EXECUTABLE)
	@echo Cleaning done
	
.PHONY: all clean
