MYSQL_SOURCE = ../../mysql960.source
MYSQL_BINARY = ../../mysql960
BOOST_DIR = $(MYSQL_SOURCE)/extra/boost/boost_1_87_0
PLUGIN_DIR = $(MYSQL_BINARY)/lib/plugin
GEOS_DIR = ../../geos
GEOS_BUILD = $(GEOS_DIR)/build

CXX = g++
CXXFLAGS = -shared -fPIC -std=c++17 -O2 -fvisibility=hidden -DMYSQL_DYNAMIC_PLUGIN \
           -I $(MYSQL_SOURCE)/build/include \
           -I $(MYSQL_BINARY)/include \
           -I $(MYSQL_SOURCE)/include \
           -I $(MYSQL_SOURCE) \
           -I $(MYSQL_SOURCE)/build \
           -I $(BOOST_DIR) \
           -I $(GEOS_DIR)/include \
           -I $(GEOS_BUILD)/capi \
           -I $(GEOS_DIR)/capi \
           -I ..

TARGET = spatial_plugin.so
SOURCES = spatial_plugin.cc ../gis_lib/wkb_parser.cc

MYSQL = $(MYSQL_BINARY)/bin/mysql --socket=$(MYSQL_BINARY)/mysql.sock -u root

-include local.mk

.PHONY: all install clean generate-srids test reload geos

all: $(TARGET)

$(TARGET): $(SOURCES) ../gis_lib/wkb_parser.h ../gis_lib/geometry_types.h ../gis_lib/geographic_srids.h plugin_version.h
	@if [ ! -f $(GEOS_BUILD)/lib/libgeos.a ]; then \
		echo "ERROR: libgeos.a not found. Run 'make geos' first to build GEOS."; \
		exit 1; \
	fi
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES) \
		-L $(MYSQL_BINARY)/lib -lmysqlservices \
		-L $(GEOS_BUILD)/lib -lgeos_c -lgeos -lstdc++ -lm

geos:
	@echo "Building GEOS static library..."
	mkdir -p $(GEOS_BUILD)
	cmake -S $(GEOS_DIR) -B $(GEOS_BUILD) \
		-DBUILD_SHARED_LIBS=OFF \
		-DBUILD_TESTING=OFF \
		-DBUILD_BENCHMARKS=OFF \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON
	cmake --build $(GEOS_BUILD) -j$$(nproc)
	@echo "GEOS build complete: $(GEOS_BUILD)/lib/libgeos.a"

install: $(TARGET)
	cp $(TARGET) $(PLUGIN_DIR)/

generate-srids:
	../gis_lib/generate_srids.sh "$(MYSQL)"

reload: install
	$(MYSQL) -e "UNINSTALL PLUGIN spatial_plugin; INSTALL PLUGIN spatial_plugin SONAME 'spatial_plugin.so';"

test:
	$(MYSQL) < test.sql

clean:
	rm -f $(TARGET)
