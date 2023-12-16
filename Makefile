CXX = clang++-16
CLANG_FORMAT = clang-format-16

install: build
	sudo cmake --build build --config Release --target install -v

build: setup
	cmake --build build --verbose

setup:
	if [ ! -d build ]; then \
	CXX=$(CXX) cmake -B build -DCLANG_FORMAT=$(CLANG_FORMAT); \
	fi

format: setup
	cmake --build build --target clang-format --verbose

cmake-format:
	find . -path ./build -prune -false -o \( -name CMakeLists.txt -o -name '*.cmake' \) \
	| xargs -n 1 cmake-format -i

clean:
	@rm -rf build

.PHONY: install build format cmake-format clean
