#!/usr/bin/env python
# -*- coding: utf-8 -*-

import io
import os
from setuptools import setup  # type: ignore
# pylint: disable=wrong-import-order,import-error,no-name-in-module
from distutils.core import Extension
# pylint: enable=wrong-import-order,import-error,no-name-in-module

bplist = Extension('bpylist.bplist', sources=['src/bplist.c'])

here = os.path.abspath(os.path.dirname(__file__))
with io.open(os.path.join(here, 'README.rst'), encoding='utf-8') as f:
    long_description = '\n' + f.read()

setup(
    name='bpylist2',
    version='2.0.3',
    description=("parse and generate binary plists and "
                 "NSKeyedArchiver archives"),
    long_description=long_description,
    author='Marketcircle Inc., Ievgen Varavva',
    author_email='yvaravva@google.com',
    url='https://github.com/xa4a/bpylist2',
    ext_modules=[
        bplist,
    ],
    packages=[
        'bpylist',
    ],
    setup_requires=[
        "pycodestyle==2.3.1",
        "pytest-runner",
        "pytest-pylint",
        "pytest-codestyle",
        "pytest-flake8==1.0.1",
        "pytest-mypy",
        'dataclasses;python_version<"3.7"',
    ],
    tests_require=["pytest"],
    install_requires=[
        'dataclasses;python_version<"3.7"',
    ],
    include_package_data=True,
    test_suite='tests',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Programming Language :: Python :: 3.6',
        'Intended Audience :: Developers',
        'Topic :: Software Development :: Libraries'
    ]

)
