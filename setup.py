#!/usr/bin/env python
# -*- coding: utf-8 -*-

from setuptools import setup
from distutils.core import Extension

bplist = Extension('bpylist.bplist', sources=['src/bplist.c'])

setup(
    name='bpylist',
    version='0.1.0',
    author='Marketcircle Inc.',
    author_email='cloud@marketcircle.com',
    url='https://github.com/marketcircle/bpylist',
    ext_modules=[
        bplist,
    ],
    packages=[
        'bpylist',
    ],
 include_package_data=True,
    
    test_suite='tests'
)
