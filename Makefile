SRC_DIR=src/
BUILD_DIR=build/

CFLAGS=-Wall -std=c++17

TARGET = $(BUILD_DIR)rcpspt-exact
OBJS:=$(BUILD_DIR)Main.o $(BUILD_DIR)Problem.o $(BUILD_DIR)Parser.o $(BUILD_DIR)encoders/SmtEncoder.o

all : $(TARGET)

$(TARGET) : $(OBJS)
		g++ $(CFLAGS) -o $@ $(OBJS) /usr/local/lib/libyices.a -lgmp

$(BUILD_DIR)%.o : $(SRC_DIR)%.cc
		g++ $(CFLAGS) -c -o $@ $<

clean :
		rm -f build/*.o $(TARGET)