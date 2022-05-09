## in dir src/: file logic.hh is included by file dmc.hh

GXX = g++-9

# opt = -Ofast # be careful with inf and nan
ASSEMBLY_OPTIONS = -g -std=c++2a -Wno-register $(opt)

# link = -static # beware of segfault with std::thread
LINK_OPTIONS = -lgmpxx -lgmp -lpthread $(link) # order matters

################################################################################

CXXOPTS = libraries/cxxopts/include/cxxopts.hpp

CUDD_DIR = libraries/cudd
CUDD_LIB_DIR = $(CUDD_DIR)/cudd/.libs
CUDD_TARGET = $(CUDD_LIB_DIR)/libcudd.a
CUDD_INCLUSIONS = -I$(CUDD_DIR) -I$(CUDD_DIR)/cudd -I$(CUDD_DIR)/epd -I$(CUDD_DIR)/mtr -I$(CUDD_DIR)/st
CUDD_LINKS = -L$(CUDD_LIB_DIR) -lcudd

SYLVAN_DIR = libraries/sylvan
SYLVAN_BUILD_DIR = $(SYLVAN_DIR)/build
SYLVAN_LIB_DIR = $(SYLVAN_BUILD_DIR)/src
SYLVAN_TARGET = $(SYLVAN_LIB_DIR)/libsylvan.a
SYLVAN_INCLUSIONS = -I$(SYLVAN_DIR)/src
SYLVAN_LINKS = -L$(SYLVAN_LIB_DIR) -lsylvan

################################################################################

DMC_OBJECTS = logic.o dmc.o

.ONESHELL: # for all targets

dmc: $(DMC_OBJECTS)
	$(GXX) -o dmc $(DMC_OBJECTS) $(CUDD_LINKS) $(SYLVAN_LINKS) $(LINK_OPTIONS)

dmc.o: src/dmc.cc src/dmc.hh src/logic.hh $(CXXOPTS) $(CUDD_TARGET) $(SYLVAN_TARGET)
	$(GXX) src/dmc.cc -c $(ASSEMBLY_OPTIONS) $(CUDD_INCLUSIONS) $(SYLVAN_INCLUSIONS)

logic.o: src/logic.cc src/logic.hh
	$(GXX) src/logic.cc -c $(ASSEMBLY_OPTIONS)

$(CUDD_TARGET): $(shell find $(CUDD_DIR)/cudd -name "*.c" -o -name "*.h") $(shell find $(CUDD_DIR)/cplusplus -name "*.cc" -o -name "*.hh")
	cd $(CUDD_DIR)
	autoreconf
	./configure --silent --enable-obj
	make -s

$(SYLVAN_TARGET):
	mkdir -p $(SYLVAN_BUILD_DIR)
	cd $(SYLVAN_BUILD_DIR)
	cmake .. -DBUILD_SHARED_LIBS=off
	make -s

.PHONY: all cudd sylvan clean clean-cudd clean-sylvan clean-libraries

cudd: $(CUDD_TARGET)

sylvan: $(SYLVAN_TARGET)

clean:
	rm -f *.o dmc

clean-cudd:
	git clean -xdf $(CUDD_DIR)

clean-sylvan:
	rm -rf $(SYLVAN_BUILD_DIR)

clean-libraries: clean-cudd clean-sylvan
