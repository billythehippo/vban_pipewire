CC=g++
CFLAGS=-Wall
LDFLAGS=-ljack -lpthread
SOURCES=main.cpp \
        ../vban_common/vban_functions.cpp \
        ../vban_common/udp.cpp \
        ../vban_common/jack_backend.cpp \
        ../vban_common/vban_client_list.cpp \
        ../vban_common/zita-resampler/resampler.cc \
        ../vban_common/zita-resampler/vresampler.cc \
        ../vban_common/zita-resampler/cresampler.cc \
        ../vban_common/zita-resampler/resampler-table.cc
OBJECTS=$(SOURCES:.cpp=.o)
OBJECTS:=$(OBJECTS:.cc=.o)
TARGET=vban_receptor_jack
PREFIX=/usr/local

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o3 -o $@

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cc
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) $(PREFIX)/bin/

run:
	./$(TARGET)
