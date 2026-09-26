# Build the C2C topology test against libsophon.
#
# Defaults assume libsophon is installed under /opt/sophon. To build against an
# in-tree / custom build, override INC and LIB, e.g.:
#
#   make INC=../../include LIB=/path/to/built/bmlib
#   # then at run time: LD_LIBRARY_PATH=/path/to/built/bmlib ./test_c2c_topology
#
INC ?= /opt/sophon/libsophon-current/include
LIB ?= /opt/sophon/libsophon-current/lib

CXX      ?= g++
CXXFLAGS += -std=c++11 -O2 -Wall -I$(INC)
LDFLAGS  += -L$(LIB) -lbmlib

test_c2c_topology: test_c2c_topology.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f test_c2c_topology

.PHONY: clean
