.PHONY: test lint

# Run the unit test suite. This uses the standard library's unittest
# discovery so that tests can run without any external dependencies.
test:
	python -m unittest discover -v tests

# Run basic linting checks using the tools shipped with the
# evaluation environment. Pre-commit isn't available, so invoke the
# linters directly.
lint:
	ruff check .
	black --check .
	mypy bpylist2

