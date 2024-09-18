.PHONY: clean test coverage
clean:
	rm -r build

test:
	cmake -Bbuild
	cmake --build build
	make test -C build

coverage:
	cmake -Bbuild
	cmake --build build
	make coverage -C build