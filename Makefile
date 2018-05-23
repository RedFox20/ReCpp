
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
	sudo ln -sf /usr/lib/llvm-5.0/bin/clang++- /usr/bin/clang++

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

