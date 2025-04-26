CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I. -I/usr/include/postgresql
LDFLAGS = -lpq -lpthread

SRCS = routing_server.cpp db_manager.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = routing_server

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) 