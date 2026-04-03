CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
TARGET = app
SRCS = client.cpp md5.cpp sha1.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

client.o: client.cpp md5.h sha1.h
md5.o: md5.cpp md5.h
sha1.o: sha1.cpp sha1.h

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
