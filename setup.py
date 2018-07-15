#!/usr/bin/env python
# -*- coding: utf-8 -*-

import io
import os
from setuptools import setup
from distutils.core import Extension

bplist = Extension('bpylist.bplist', sources=['src/bplist.c'])

here = os.path.abspath(os.path.dirname(__file__))
with io.open(os.path.join(here, 'README.rst'), encoding='utf-8') as f:
    long_description = '\n' + f.read()

setup(
    name='bpylist2',
    version='2.0.1',
    description = "parse and generate binary plists and NSKeyedArchiver archives",
    long_description = long_description,
    author='Marketcircle Inc., Ievgen Varavva',
    author_email='yvaravva@google.com',
    url='https://github.com/xa4a/bpylist2',
    ext_modules=[
        bplist,
    ],
    packages=[
        'bpylist',
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
