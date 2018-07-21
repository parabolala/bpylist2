from unittest import TestCase
from tests.fixtures import get_fixture
from datetime import datetime, timezone
from bpylist.archive_types import uid, timestamp, NSMutableData
from bpylist import archiver, bplist


class FooArchive:

    def __init__(self, title, stamp, count, cats, meta, empty, recursive):
        self.title = title
        self.stamp = stamp
        self.count = count
        self.categories = cats
        self.metadata = meta
        self.empty = empty
        self.recursive = recursive

    def encode_archive(obj, archive):
        archive.encode('title', obj.title)
        archive.encode('recurse', obj.recursive)

    def decode_archive(archive):
        title   = archive.decode('title')
        stamp   = archive.decode('stamp')
        count   = archive.decode('count')
        cats    = archive.decode('categories')
        meta    = archive.decode('metadata')
        empty   = archive.decode('empty')
        recurse = archive.decode('recursive')
        return FooArchive(title, stamp, count, cats, meta, empty, recurse)

archiver.update_class_map({ 'crap.Foo': FooArchive })


class UnarchiveTest(TestCase):

    def fixture(self, name):
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

        archiver.update_class_map({ 'crap.Foo': FooArchive })

    def test_complains_about_missing_class_uid(self):
        with self.assertRaises(archiver.MissingClassUID):
            self.unarchive('missing_uid')

    def test_unpack_archive_with_null_value(self):
        foo = self.unarchive('null')
        self.assertIsNone(foo.empty)

    def test_unpack_archive_with_no_values(self):
        foo = self.unarchive('empty')
        self.assertIsNone(foo.title)
        self.assertIsNone(foo.count)
        self.assertIsNone(foo.categories)
        self.assertIsNone(foo.metadata)

    def test_unpack_simple_archive(self):
        foo = self.unarchive('simple')
        self.assertEqual('yo', foo.title)
        self.assertEqual(42, foo.count)

    def test_unpack_complex_archive(self):
        foo = self.unarchive('complex')
        self.assertEqual('yo', foo.title)
        self.assertEqual(42, foo.count)
        self.assertEqual(['banana', 'apple'], foo.categories)
        self.assertEqual({'fruit': 'kiwi', 'veg': 'asparagus'}, foo.metadata)

    def test_unpack_recursive_archive(self):
        foo = self.unarchive('recursive')
        bar = foo.recursive
        self.assertTrue('hello', bar.title)
        self.assertTrue('yo', foo.title)

    def test_unpack_date(self):
        exp = datetime(2017, 2, 23, 6, 15, 58, 684097, tzinfo=timezone.utc)
        foo = self.unarchive('date')
        act = foo.stamp.to_datetime()
        self.assertEqual(exp, act)

    def test_unpack_data(self):
        foo = self.unarchive('data')
        self.assertEqual(b'', foo.stamp)

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


class ArchiveTest(TestCase):

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
        self.archive({ 'fruit': 'kiwi', 'veg': 'asparagus' })
        self.archive(b'hello')
        self.archive({ 'data': b'hello' })
        self.archive(timestamp(0))
        self.archive([timestamp(-4)])

    def test_custom_type(self):
        foo = FooArchive('herp', timestamp(9001), 42,
                         ['strawberries', 'dragonfruit'],
                         { 'key': 'value' },
                         False,
                         None)

    def test_circular_ref(self):
        foo = FooArchive('herp', timestamp(9001), 42,
                         ['strawberries', 'dragonfruit'],
                         { 'key': 'value' },
                         False,
                         None)
        foo.recursive = foo
        plist = bplist.parse(archiver.archive(foo))
        foo_obj = plist['$objects'][1]
        self.assertEqual(uid(1), foo_obj['recurse'])




if __name__ == '__main__':
    from unittest import main
    main()
