DELETE_ON_ERROR:

venv:
	python -mvenv venv

requirements:
	python -mpip install -r requirements.txt

pybuild:
	python setup.py install --force

cbuild:
	mkdir cbuild

ctest: cbuild
	cd cbuild; cmake .. -DCMAKE_BUILD_TYPE=Debug
	cd cbuild; cmake --build .
	cd cbuild; ctest -V .

pytest:
	pytest tests

publish:
	rm -rf dist
	python3 setup.py bdist_wheel
	twine upload dist/*

.PHONY: publish venv requirements pytest pybuild ctest
.DELETE_ON_ERROR:
