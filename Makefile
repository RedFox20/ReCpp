
CLANG_FLAGS := -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
GCC_FLAGS := -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++

all: clang

configure-clang:
	sudo update-alternatives --install /usr/bin/clang   clang   /usr/bin/clang-5.0   1000
	sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-5.0 1000

configure-clang-travis:
	sudo ln -sf /usr/local/clang-5.0.0/lib/libc++.so.1    /usr/lib
	sudo ln -sf /usr/local/clang-5.0.0/lib/libc++abi.so.1 /usr/lib
	sudo ln -sf /usr/bin/clang-5.0   /usr/bin/clang
	sudo ln -sf /usr/bin/clang++-5.0 /usr/bin/clang++

configure-clang-semaphore:
	sudo ln -sf /usr/lib/x86_64-linux-gnu/libc++.so.1    /usr/lib
	sudo ln -sf /usr/lib/x86_64-linux-gnu/libc++abi.so.1 /usr/lib
	sudo ln -sf /usr/lib/llvm-5.0/bin/clang   /usr/bin/clang
	sudo ln -sf /usr/lib/llvm-5.0/bin/clang++ /usr/bin/clang++

configure-gcc-travis: configure-gcc
configure-gcc:
	sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60
	sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-7 60

configure-gcc-semaphore:
	sudo ln -sf /usr/bin/gcc-7 /usr/bin/gcc
	sudo ln -sf /usr/bin/g++-7 /usr/bin/g++

configure-clang5:
	sudo wget http://ateh10.net/dev/clang++5.zip -P /tmp
	sudo unzip /tmp/clang++5.zip -d /usr/local
	sudo rm -f /tmp/clang++5.zip
	sudo ln -sf /usr/local/clang++5/lib/libc++.so.1    /usr/lib
	sudo ln -sf /usr/local/clang++5/lib/libc++abi.so.1 /usr/lib
	sudo ln -sf /usr/local/clang++5/bin/clang      /usr/bin/clang
	sudo ln -sf /usr/local/clang++5/bin/clang++    /usr/bin/clang++
	sudo ln -sf /usr/local/clang++5/include/c++/v1 /usr/include/c++/v1

configure-python:
	sudo mkdir -p /opt/pip
	sudo wget https://bootstrap.pypa.io/get-pip.py -P /opt/pip
	sudo python3.6 /opt/pip/get-pip.py

configure-cmake:
	sudo mkdir -p /opt/cmake
	sudo wget https://cmake.org/files/v3.11/cmake-3.11.2-Linux-x86_64.sh -P /opt/cmake
	sudo sh /opt/cmake/cmake-3.11.2-Linux-x86_64.sh --prefix=/opt/cmake --skip-license
	sudo ln -sf /opt/cmake/bin/cmake /usr/bin/cmake

/usr/local/clang++5: configure-clang5
/opt/pip: configure-python
/opt/cmake: configure-cmake
configure-mama: /opt/pip /opt/cmake /usr/local/clang+llvm-5.0
	sudo pip install mama

clang: build
	cd build && cmake ../ -DCMAKE_BUILD_TYPE=RelWithDebInfo $(CLANG_FLAGS) && make -j4

gcc: build
	cd build && cmake ../ -DCMAKE_BUILD_TYPE=RelWithDebInfo $(GCC_FLAGS) && make -j4

build:
	mkdir build

