UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CTESTFLAGS += -T memcheck
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
	cd cbuild; cmake .. -DCMAKE_BUILD_TYPE=Debug
	cd cbuild; cmake --build .
	cd cbuild; ctest -V --output-on-failure $(CTESTFLAGS) .

pytest:
	pytest tests

publish:
	rm -rf dist
	python setup.py bdist bdist_wheel
	twine upload dist/*

.PHONY: publish venv requirements pytest pybuild ctest
.DELETE_ON_ERROR:
