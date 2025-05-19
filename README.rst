bpylist2 |pypi version| |Build Status|
======================================

This is a fork of Marketcircle/bpylist. This one is hopefully more responsive to PRs.

Implementation of the `Apple's Binary
Plist <https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man5/plist.5.html>`__
and the NSKeyedArchiver format

Usage
-----

Binary Plists
~~~~~~~~~~~~~

For reading and writing plain PLists please use stdlib `plistlib` library.

KeyedArchives
~~~~~~~~~~~~~

``NSKeyedArchiver`` is an Apple proprietary serialization format for
Cocoa objects. ``bpylist2`` supports reading and writing
``NSKeyedArchiver`` compatible archives. The API is similar to the
binary plist API.

**Unarchiving an object**

.. code:: python

    from bpylist2 import archiver

    with open('my_archived_object', 'rb') as f:
        archiver.unarchive(f.read())

**Archiving an object**

.. code:: python

    from bpylist2 import archiver

    my_object = { 'foo':'bar', 'some_array': [1,2,3,4] }
    archiver.archive(my_object)

Custom objects
^^^^^^^^^^^^^^

If your archive includes classes that are not "standard" Cocoa classes
(``NSString``, ``NSNumber``, ``NSDate``, ``NSNull``, ``NSDictionary`` or
``NSArray``), you register a Python class that the Cocoa class maps to and
register it.

The simplest way to define a class is by providing a python dataclass, for
example you define a class with all the fields of the archived object:

.. code:: python

    @dataclasses.dataclass
    class MyClass(DataclassArchiver):
        int_field: int = 0
        str_field: str = ""
        float_field: float = -1.1
        list_field: list = dataclasses.field(default_factory=list)

Alternatively you can implement custom unarchiving code.  

The Python class needs to implement the ``encode_archive`` and
``decode_archive`` methods.

.. code:: python

    ## Define a Python Class

    from bpylist2 import archiver

    class MyClass:
        first_property = None
        second_property = None

        def __init__(self, first_property, second_property):
            self.first_property = first_property
            self.second_property = second_property

        def encode_archive(self, archive):
            archive.encode('first_property', self.first_property)
            archive.encode('second_property', self.second_property)

        def decode_archive(archive):
            first = archive.decode('first_property')
            second = archive.decode('second_property')
            return MyClass(first, second)

When the mapper class is defined, register it with unarchiver:

.. code:: python

    ## Register the class for the Cocoa class 'MyCocoaClass'

    archiver.update_class_map({ 'MyCocoaClass': FooArchive })

Implementation Note
-------------------

This package requires the version of `plistlib` included in the Python 3.8 
standard library.  In order to support Python 3.6 and 3.7, a copy of the 
Python 3.8 `plistlib <https://github.com/python/cpython/blob/e51dd9dad6590bf3a940723fbbaaf4f64a3c9228/Lib/plistlib.py>`__ 
is bundled with `bpylist2` (Specifically, commit `9054967 <https://github.com/python/cpython/commit/90549676e063c2c818cfc14213d3adb7edcc2bd5>`__).  
This version will only be used if `bpylist2` detects it is running on Python < 3.8.

Running tests
-------------

The test suite uses the standard library's ``unittest`` module and can be
executed with ``make``::

    $ make test

Lint checks are available via ``make lint``.

How to publish a new version to PyPI
------------------------------------

.. code-block:: bash

    $ poetry build 
    $ poetry publish

License
-------

MIT License

Copyright (c) 2017 Marketcircle Inc.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

.. |pypi version| image:: https://img.shields.io/pypi/v/bpylist2.svg
   :target: https://pypi.org/project/bpylist2/
.. |Build Status| image:: https://travis-ci.org/parabolala/bpylist2.svg?branch=master
   :target: https://travis-ci.org/parabolala/bpylist2
