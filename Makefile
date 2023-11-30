install: build
	sudo cmake --build build --target install -v

build:
	@rm -rf build
	cmake -B build
	cmake --build build --verbose

clean:
	@rm -rf build

.PHONY: install build clean
