INCLUDES=-Iflatritrie -Ireference

.PHONY: unit_tests benchmark release all

all: unit_tests release

ifdef RELEASE
CFLAGS=-std=c++17 -Wall -O3 -DNDEBUG
else
CFLAGS=-std=c++17 -Wall -O0 -ggdb
endif

unit_tests:
	g++ $(CFLAGS) $(INCLUDES) -o unit_tests unit_tests.cpp
	./unit_tests

benchmark:
	g++ $(CFLAGS) $(INCLUDES) -o benchmark benchmark.cpp
	./benchmark

geoip:
	g++ $(CFLAGS) $(INCLUDES) -o example_geoip example_geoip.cpp
