#!/usr/bin/env python
# -*- coding: utf-8 -*-

import io
import os
from setuptools import setup  # type: ignore

here = os.path.abspath(os.path.dirname(__file__))
with io.open(os.path.join(here, 'README.rst'), encoding='utf-8') as f:
    long_description = '\n' + f.read()

setup(
    name='bpylist2',
    version='3.0.3',
    description=("Parse and generate NSKeyedArchiver archives"),
    long_description=long_description,
    author='Marketcircle Inc., Ievgen Varavva',
    author_email='fuzzy.parabola@gmail.com',
    url='https://github.com/parabolala/bpylist2',
    packages=[
        'bpylist',
    ],
    setup_requires=[
        "pytest-runner==5.3.0",
        "pytest-pylint==0.18.0",
        "pytest-pycodestyle==2.2.0",
        "pytest-mypy==0.8.1",
        'dataclasses;python_version<"3.7"',
    ],
    tests_require=["pytest==6.2.3"],
    install_requires=[
        'dataclasses;python_version<"3.7"',
    ],
    include_package_data=True,
    test_suite='tests',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Programming Language :: Python :: 3.8',
        'Intended Audience :: Developers',
        'Topic :: Software Development :: Libraries'
    ],

    # 3.8 required for plistlib.UID.
    # Includes local copy of 3.8's plistlib for < 3.8
    python_requires=">=3.6",
)
