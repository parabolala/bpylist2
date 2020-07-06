import dataclasses
from datetime import datetime, timezone
import sys
import unittest

from bpylist import archiver, archive_types
from bpylist.archive_types import timestamp, NSMutableData
from tests.fixtures import get_fixture

if sys.version_info < (3, 8, 0):
    # pylint: disable=ungrouped-imports
    from bpylist import _plistlib as plistlib
    # pylint: enable=ungrouped-imports
else:
    import plistlib  # type: ignore


class FooArchive:

    def __init__(self, title, stamp, count, cats, meta, empty, recursive):
        self.title = title
        self.stamp = stamp
        self.count = count
        self.categories = cats
        self.metadata = meta
        self.empty = empty
        self.recursive = recursive

    def __eq__(self, other):
        for field in ['title', 'stamp', 'count', 'categories', 'metadata',
                      'empty', 'recursive']:
            if getattr(self, field) != getattr(other, field):
                return False
        return True

    @staticmethod
    def encode_archive(obj, archive):
        archive.encode('title', obj.title)
        archive.encode('stamp', obj.stamp)
        archive.encode('count', obj.count)
        archive.encode('categories', obj.categories)
        archive.encode('metadata', obj.metadata)
        archive.encode('empty', obj.empty)
        archive.encode('recurse', obj.recursive)

    @staticmethod
    def decode_archive(archive):
        title = archive.decode('title')
        stamp = archive.decode('stamp')
        count = archive.decode('count')
        cats = archive.decode('categories')
        meta = archive.decode('metadata')
        empty = archive.decode('empty')
        recurse = archive.decode('recursive')
        return FooArchive(title, stamp, count, cats, meta, empty, recurse)


archiver.update_class_map({'crap.Foo': FooArchive})


@dataclasses.dataclass
class FooDataclass(archive_types.DataclassArchiver):
    int_field: int = 0
    str_field: str = ""
    float_field: float = -1.1
    list_field: list = dataclasses.field(default_factory=list)


@dataclasses.dataclass
class DataclassMussingFields(archive_types.DataclassArchiver):
    int_field: int = 0


archiver.update_class_map({
    'FooDataclass': FooDataclass,
})


class UnarchiveTest(unittest.TestCase):

    @staticmethod
    def fixture(name):
        return get_fixture(f'{name}_archive.plist')

    def unarchive(self, plist):
        return archiver.unarchive(self.fixture(plist))

    def test_complains_about_incorrect_archive_type(self):
        with self.assertRaises(archiver.UnsupportedArchiver):
            self.unarchive('invalid_type')

    def test_complains_about_incorrect_version(self):
        with self.assertRaises(archiver.UnsupportedArchiveVersion):
            self.unarchive('invalid_version')

    def test_complains_about_missing_top_object(self):
        with self.assertRaises(archiver.MissingTopObject):
            self.unarchive('no_top')

    def test_complains_about_missing_top_object_uid(self):
        with self.assertRaises(archiver.MissingTopObjectUID):
            self.unarchive('no_root')

    def test_complains_about_missing_objects(self):
        with self.assertRaises(archiver.MissingObjectsArray):
            self.unarchive('no_objects')

    def test_complains_about_missing_class_metadata(self):
        with self.assertRaises(archiver.MissingClassMetaData):
            self.unarchive('no_class_meta')

    def test_complains_about_missing_class_names(self):
        with self.assertRaises(archiver.MissingClassName):
            self.unarchive('no_class_name')

    def test_complains_about_unmapped_classes(self):
        del archiver.UNARCHIVE_CLASS_MAP['crap.Foo']

        with self.assertRaises(archiver.MissingClassMapping):
            self.unarchive('simple')

        archiver.update_class_map({'crap.Foo': FooArchive})

    def test_complains_about_missing_class_uid(self):
        with self.assertRaises(archiver.MissingClassUID):
            self.unarchive('missing_uid')

    def test_unpack_archive_with_null_value(self):
        obj = self.unarchive('null')
        self.assertIsNone(obj.empty)

    def test_unpack_archive_with_no_values(self):
        obj = self.unarchive('empty')
        self.assertIsNone(obj.title)
        self.assertIsNone(obj.count)
        self.assertIsNone(obj.categories)
        self.assertIsNone(obj.metadata)

    def test_unpack_simple_archive(self):
        obj = self.unarchive('simple')
        self.assertEqual('yo', obj.title)
        self.assertEqual(42, obj.count)

    def test_unpack_complex_archive(self):
        obj = self.unarchive('complex')
        self.assertEqual('yo', obj.title)
        self.assertEqual(42, obj.count)
        self.assertEqual(['banana', 'apple'], obj.categories)
        self.assertEqual({'fruit': 'kiwi', 'veg': 'asparagus'}, obj.metadata)

    def test_unpack_recursive_archive(self):
        obj = self.unarchive('recursive')
        inner = obj.recursive
        self.assertEqual('yo', inner.title)
        self.assertEqual('hello', obj.title)

    def test_unpack_date(self):
        exp = datetime(2017, 2, 23, 6, 15, 58, 684097, tzinfo=timezone.utc)
        obj = self.unarchive('date')
        act = obj.stamp.to_datetime()
        self.assertEqual(exp, act)

    def test_unpack_data(self):
        obj = self.unarchive('data')
        self.assertEqual(b'', obj.stamp)

    def test_unpack_circular_ref(self):
        with self.assertRaises(archiver.CircularReference):
            self.unarchive('circular')

    def test_unpack_primitive_multiple_refs(self):
        expected = ['a', 'a']
        actual = archiver.unarchive(archiver.archive(['a', 'a']))
        self.assertEqual(actual, expected)

    def test_unpack_nsmutabledata(self):
        expected = NSMutableData(b'hello')
        actual = self.unarchive('nsmutabledata')
        self.assertEqual(actual, expected)

    def test_dataclass_unarchiver(self):
        expected = FooDataclass(
            int_field=5, str_field='hello', float_field=3.15,
            list_field=['foo', 'bar', 'baz']
        )
        actual = self.unarchive('dataclass')
        self.assertEqual(actual, expected)

    def test_dataclass_not_fully_mapped(self):
        archiver.update_class_map({
            'FooDataclass': DataclassMussingFields,
        })
        try:
            with self.assertRaises(archive_types.Error):
                self.unarchive('dataclass')
        finally:
            # Restore mapping.
            archiver.update_class_map({
                'FooDataclass': FooDataclass,
            })


class ArchiveTest(unittest.TestCase):

    def archive(self, obj):
        archived = archiver.archive(obj)
        unarchived = archiver.unarchive(archived)
        self.assertEqual(obj, unarchived)

    def test_primitive(self):
        self.archive(True)
        self.archive(9001)
        self.archive('banana')

    def test_core_types(self):
        self.archive(1)
        self.archive('two')
        self.archive(3.14)
        self.archive([1, 'two', 3.14])
        self.archive({'fruit': 'kiwi', 'veg': 'asparagus'})
        self.archive(b'hello')
        self.archive({'data': b'hello'})
        self.archive(timestamp(0))
        self.archive([timestamp(-4)])

    def test_custom_type(self):
        obj = FooArchive('herp', timestamp(9001), 42,
                         ['strawberries', 'dragonfruit'],
                         {'key': 'value'},
                         False,
                         None)
        self.archive(obj)

    def test_circular_ref(self):
        obj = FooArchive('herp', timestamp(9001), 42,
                         ['strawberries', 'dragonfruit'],
                         {'key': 'value'},
                         False,
                         None)
        obj.recursive = obj
        plist = plistlib.loads(archiver.archive(obj))
        foo_obj = plist['$objects'][1]
        self.assertEqual(plistlib.UID(1), foo_obj['recurse'])

    def test_dataclass(self):
        obj = FooDataclass(
            int_field=15, str_field='hello there', float_field=3.13,
            list_field=['foo', 'baz']
        )
        self.archive(obj)


if __name__ == '__main__':
    unittest.main()
