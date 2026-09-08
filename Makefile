.PHONY: all build install clean run

all: build

build:
	cmake -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build -j$$(nproc)

install: build
	cmake --install build

clean:
	rm -rf build

run: build
	./build/screen-recorder

