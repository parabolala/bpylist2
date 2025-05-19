import json
from typing import Iterator

from os.path import dirname, join


def get_fixture(name: str) -> bytes:
    fixture_dir = join(dirname(__file__), "fixture_data")
    fixture_file = join(fixture_dir, name)
    with open(fixture_file, "rb") as f:
        return f.read()


def naughty_strings() -> Iterator[str]:
    data = json.loads(get_fixture("naughty-strings.json").decode("utf-8"))
    for string in data:
        yield string


def full_change_log_bplist() -> bytes:
    return get_fixture("fullChangeLog.bplist")
