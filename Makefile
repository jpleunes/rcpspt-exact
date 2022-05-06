SRC_DIR=src/
BUILD_DIR=build/

CFLAGS=-Wall -std=c++17

TARGET = $(BUILD_DIR)rcpspt-exact
OBJS:=$(BUILD_DIR)Main.o $(BUILD_DIR)Problem.o

all : $(TARGET)

$(TARGET) : $(OBJS)
		g++ $(CFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)%.o : $(SRC_DIR)%.cc
		g++ $(CFLAGS) -c -o $@ $<

clean :
		rm -f build/*.o $(TARGET)
