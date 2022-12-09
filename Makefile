UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	MEMCHECK_FLAGS += -T memcheck
endif

venv:
	python -mvenv venv

requirements:
	python -mpip install --upgrade pip
	python -mpip install -r requirements.txt

pybuild:
	python setup.py install --force

cbuild:
	mkdir cbuild

ctest: cbuild
	cd cbuild; cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=On
	cd cbuild; cmake --build . -j
	cd cbuild; ctest -V --output-on-failure $(MEMCHECK_FLAGS) .

covtest: cbuild
	cd cbuild; cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=On -DENABLE_COVERAGE=On
	cd cbuild; cmake --build . -j
	cd cbuild; ctest -V --output-on-failure .

tsanbuild:
	mkdir tsanbuild

tsantest: tsanbuild
	cd tsanbuild; cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=On -DENABLE_TSAN=On
	cd tsanbuild; cmake --build . -j
	cd tsanbuild; ctest -V --output-on-failure .

pytest:
	pytest tests

publish:
	rm -rf dist
	python setup.py bdist bdist_wheel
	twine upload dist/*

.PHONY: publish venv requirements pytest pybuild ctest
.DELETE_ON_ERROR:
