
CLANG_FLAGS := -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
GCC_FLAGS := -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++

all: clang

configure-clang:
	sudo update-alternatives --install /usr/bin/clang   clang   /usr/bin/clang-5.0   1000
	sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-5.0 1000

configure-clang-travis:
	sudo update-alternatives --install /usr/bin/clang   clang   /usr/bin/clang-5.0   1000
	sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-5.0 1000
	sudo update-alternatives --set clang   /usr/bin/clang-5.0
	sudo update-alternatives --set clang++ /usr/bin/clang++-5.0
	whereis clang clang++
	clang++ --version

configure-gcc-travis: configure-gcc
configure-gcc:
	sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60
	sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-7 60

clang: build
	cd build && cmake ../ -DCMAKE_BUILD_TYPE=RelWithDebInfo $(CLANG_FLAGS) && make -j4

gcc: build
	cd build && cmake ../ -DCMAKE_BUILD_TYPE=RelWithDebInfo $(GCC_FLAGS) && make -j4

build:
	mkdir build

