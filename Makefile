# This is a Legacy Makefile for CI setup. It is not intended for local development.
CLANG_FLAGS := -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
GCC_FLAGS := -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++
all: clang
clang: build
	cd build && cmake ../ -DCMAKE_BUILD_TYPE=RelWithDebInfo $(CLANG_FLAGS) && make -j4
gcc: build
	cd build && cmake ../ -DCMAKE_BUILD_TYPE=RelWithDebInfo $(GCC_FLAGS) && make -j4
build:
	mkdir build

/opt/pip: configure-pip
configure-pip: configure-pip3
configure-pip3:
	sudo mkdir -p /opt/pip
	sudo wget https://bootstrap.pypa.io/get-pip.py -P /opt/pip
	python3 -V
	sudo python3 /opt/pip/get-pip.py

/opt/cmake: configure-cmake
configure-cmake:
	sudo mkdir -p /opt/cmake
	sudo wget https://cmake.org/files/v3.30/cmake-3.30.9-linux-x86_64.sh -P /opt/cmake
	sudo sh /opt/cmake/cmake-3.30.9-linux-x86_64.sh --prefix=/opt/cmake --skip-license
	sudo ln -sf /opt/cmake/bin/cmake /usr/bin/cmake
