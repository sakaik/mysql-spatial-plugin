MYSQL_SOURCE = ../../mysql960.source
MYSQL_BINARY = ../../mysql960
BOOST_DIR = $(MYSQL_SOURCE)/extra/boost/boost_1_87_0
PLUGIN_DIR = $(MYSQL_BINARY)/lib/plugin

CXX = g++
CXXFLAGS = -shared -fPIC -std=c++17 -O2 \
           -I $(MYSQL_SOURCE)/build/include \
           -I $(MYSQL_BINARY)/include \
           -I $(MYSQL_SOURCE)/include \
           -I $(MYSQL_SOURCE) \
           -I $(MYSQL_SOURCE)/build \
           -I $(BOOST_DIR) \
           -I ..

TARGET = spatial_plugin.so
SOURCES = spatial_plugin.cc ../gis_lib/wkb_parser.cc

MYSQL = $(MYSQL_BINARY)/bin/mysql --socket=$(MYSQL_BINARY)/mysql.sock -u root

-include local.mk

.PHONY: all install clean generate-srids test reload

all: $(TARGET)

$(TARGET): $(SOURCES) ../gis_lib/wkb_parser.h ../gis_lib/geometry_types.h ../gis_lib/geographic_srids.h plugin_version.h
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES) -L $(MYSQL_BINARY)/lib -lmysqlservices

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
