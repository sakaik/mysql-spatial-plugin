MYSQL_SOURCE = ../../mysql970.source
MYSQL_BINARY = ../../mysql970
BOOST_DIR = $(MYSQL_SOURCE)/extra/boost/boost_1_87_0
PLUGIN_DIR = $(MYSQL_BINARY)/lib/plugin
GEOS_DIR = ../../geos
GEOS_BUILD = $(GEOS_DIR)/build

# libmysqlservices: built from the MySQL source tree, not taken from the binary
# distribution. The 9.7 binary dist ships libmysqlservices.a compiled with LTO
# (GCC 14 bytecode), which g++ 13/11 cannot link. The source build (WITH_LTO=OFF)
# produces a plain-object archive that links with any recent GCC.
# Build it once with: (cd $(MYSQL_SOURCE)/build && cmake .. -DDOWNLOAD_BOOST=0 -DWITH_CURL=0 && make mysqlservices)
MYSQL_SERVICES_DIR = $(MYSQL_SOURCE)/build/libservices

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

.PHONY: all install clean generate-srids test reload geos docker-build-ol9 docker-build-ubuntu2404 docker-test-mysql97

all: $(TARGET)

$(TARGET): $(SOURCES) ../gis_lib/wkb_parser.h ../gis_lib/geometry_types.h ../gis_lib/geographic_srids.h plugin_version.h
	@if [ ! -f $(GEOS_BUILD)/lib/libgeos.a ]; then \
		echo "ERROR: libgeos.a not found. Run 'make geos' first to build GEOS."; \
		exit 1; \
	fi
	@if [ ! -f $(MYSQL_SERVICES_DIR)/libmysqlservices.a ]; then \
		echo "ERROR: $(MYSQL_SERVICES_DIR)/libmysqlservices.a not found."; \
		echo "Build it: (cd $(MYSQL_SOURCE)/build && cmake .. -DDOWNLOAD_BOOST=0 -DWITH_CURL=0 && make mysqlservices)"; \
		exit 1; \
	fi
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES) \
		-L $(MYSQL_SERVICES_DIR) -lmysqlservices \
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
	# rm first so cp creates a fresh inode: overwriting a currently-loaded .so
	# in place corrupts its mmap'd pages and crashes a subsequent UNINSTALL.
	rm -f $(PLUGIN_DIR)/$(TARGET)
	cp $(TARGET) $(PLUGIN_DIR)/

generate-srids:
	../gis_lib/generate_srids.sh "$(MYSQL)"

reload: install
	$(MYSQL) --force -e "UNINSTALL PLUGIN spatial_plugin; INSTALL PLUGIN spatial_plugin SONAME 'spatial_plugin.so';"

test:
	$(MYSQL) < test.sql

clean:
	rm -f $(TARGET)

# --- Docker release builds (built against MySQL 9.7) ---
docker-build-ol9:
	./docker-build-ol9.sh

docker-build-ubuntu2404:
	./docker-build-ubuntu2404.sh

# --- Run the test suite against MySQL 9.7 inside a container ---
docker-test-mysql97:
	./docker-test-mysql97.sh

# --- Multi-version MySQL support -------------------------------------------
# Build and release spatial_plugin for a specific MySQL version. Uses
# extract-buildkit.sh, docker-build.sh, and docker-test.sh together.
#
#   make build-mysql   MYSQL_VER=26.7.0          # extract + build + test
#   make release-mysql MYSQL_VER=26.7.0          # upload to current release (latest tag)
#   make release-mysql MYSQL_VER=26.7.0 RELEASE_TAG=v0.3.0   # override target tag

MYSQL_ARTIFACT_DIR = ../../build-artifacts/$(MYSQL_VER)
SO_GLIBC_234 = $(MYSQL_ARTIFACT_DIR)/spatial_plugin-mysql-$(MYSQL_VER)-glibc-2.34.so
SO_GLIBC_239 = $(MYSQL_ARTIFACT_DIR)/spatial_plugin-mysql-$(MYSQL_VER)-glibc-2.39.so

.PHONY: check-mysql-ver build-mysql release-mysql

check-mysql-ver:
	@[ -n "$(MYSQL_VER)" ] || { \
	    echo "ERROR: MYSQL_VER is required (e.g. make build-mysql MYSQL_VER=26.7.0)"; \
	    exit 1; }

build-mysql: check-mysql-ver
	./extract-buildkit.sh $(MYSQL_VER)
	./docker-build.sh $(MYSQL_VER)
	./docker-test.sh $(MYSQL_VER) $(SO_GLIBC_234) $(SO_GLIBC_239)
	@echo
	@echo "==> Build and test succeeded. Artifacts:"
	@ls -lh $(MYSQL_ARTIFACT_DIR)/
	@echo
	@echo "    If everything looks good, upload with:"
	@echo "      make release-mysql MYSQL_VER=$(MYSQL_VER)"

release-mysql: check-mysql-ver
	@[ -f "$(SO_GLIBC_234)" ] && [ -f "$(SO_GLIBC_239)" ] || { \
	    echo "ERROR: artifacts missing. Run 'make build-mysql MYSQL_VER=$(MYSQL_VER)' first."; \
	    exit 1; }
	@TAG="$(RELEASE_TAG)"; \
	if [ -z "$$TAG" ]; then \
	    TAG=$$(gh release list --limit 1 --json tagName -q '.[0].tagName' 2>/dev/null); \
	    [ -n "$$TAG" ] || { echo "ERROR: no GitHub release found"; exit 1; }; \
	fi; \
	echo "==> Uploading to release: $$TAG"; \
	echo "    (no --clobber; will fail if asset name already exists on that release)"; \
	gh release upload "$$TAG" "$(SO_GLIBC_234)" "$(SO_GLIBC_239)"
