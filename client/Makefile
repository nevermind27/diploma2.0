.PHONY: install test run clean

install:
	python3 -m venv venv
	. venv/bin/activate && pip install -r requirements.txt

test:
	. venv/bin/activate && python -m pytest

run:
	. venv/bin/activate && python client_web.py

clean:
	rm -rf venv
	rm -rf __pycache__
	rm -rf .pytest_cache 