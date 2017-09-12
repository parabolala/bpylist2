# bpylist

Implementation of the [Apple's Binary Plist](https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man5/plist.5.html) and the NSKeyedArchiver format

## Usage

### Binary Plists

Generating bplist is easy, and similar to the `plistlib` module in Python's Standard Library

```python
from bpylist import bplist

bpylist.generate(my_object)
```

Reading is easy as well. The `generate` function takes a bytes object and returns the top-level object of the binary plist. 

```python
from bpylist import bplist
with open('myplist.plist', 'rb') as f:
    bpylist.parse(f.read())
```

### KeyedArchives

`NSKeyedArchiver` is an Apple proprietary serialization format for Cocoa objects. `bpylist` supports reading and writing `NSKeyedArchiver` compatible archives. The API is similar to the binary plist API.

**Unarchiving an object**

```
from bpylist import archiver

with open('my_archived_object', 'rb') as f:
    archiver.unarchive(f.read())
```

**Archiving an object**

```
from bpylist import archiver

my_object = { 'foo':'bar', 'some_array': [1,2,3,4] }
archiver.archive(my_object)
```


#### Custom objects

If you archive includes classes that are not "standard" Cocoa classes (`NSString`, `NSNumber`, `NSDate`, `NSNull`, `NSDictionary` or `NSArray`), you register a Python class that the Cocoa class maps to. The Python class needs to implement the `encode_archive` and `decode_archive` methods.


```
## Define a Python Class

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

## Register the class for the Cocoa class 'MyCocoaClass'

archiver.update_class_map({ 'MyCocoaClass': FooArchive })
```


## License

MIT License

Copyright (c) 2017 Marketcircle Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.