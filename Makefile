CXX = clang++-16
CLANG_FORMAT = clang-format-16

install: build
	sudo cmake --build build --config Release --target install -v

build:
	@rm -rf build
	CXX=$(CXX) cmake -B build
	cmake --build build --verbose

format:
	if [ ! -d build ]; then CXX=$(CXX) cmake -B build; fi
	FORMAT=$(CLANG_FORMAT) cmake --build build --target clang-format-src -v

cmake-format:
	cmake-format -i CMakeLists.txt

clean:
	@rm -rf build

.PHONY: install build format cmake-format clean
