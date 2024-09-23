.PHONY: clean test coverage asan


test:
	cmake -Bbuild
	cmake --build build
	make test -C build

clean:
	rm -r build

coverage:
	cmake -Bbuild
	cmake --build build
	make coverage -C build

asan:
	cmake -Bbuild -DASAN=ON
	cmake --build build
	make test -C build