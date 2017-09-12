from unittest import TestCase
from tests.fixtures import get_fixture
from datetime import datetime, timezone
from bpylist import bplist
from bpylist.archive_types import uid
import plistlib
import cmath

class BPListTest(TestCase):

    def fixture(self, name):
        return get_fixture('{name}.plist'.format(name=name))

class TestBPlistParsing(BPListTest):

    def parse(self, plist):
        return bplist.parse(self.fixture(plist))

    def test_parses_equivalent_to_plistlib(self):
        data = self.fixture('AccessibilityDefinitions')
        self.assertDictEqual(plistlib.loads(data), bplist.parse(data))

    def test_true(self):
        self.assertTrue(self.parse('true'))

    def test_false(self):
        self.assertFalse(self.parse('false'))

    def test_int8(self):
        self.assertEqual(42, self.parse('int8'))

    def test_int16(self):
        self.assertEqual(9001, self.parse('int16'))

    def test_int32(self):
        self.assertEqual(80_000, self.parse('int32'))

    def test_int64(self):
        self.assertEqual(5_000_000_000, self.parse('int64'))

    def test_int64_max(self):
        self.assertEqual(9_223_372_036_854_775_807, self.parse('int64_max'))

    def test_negative_int(self):
        self.assertEqual(-26, self.parse('negative26'))

    def test_negative_min(self):
        self.assertEqual(-1, self.parse('negative1'))

    def test_negative_max(self):
        expected = -9_223_372_036_854_775_807
        self.assertEqual(expected, self.parse('negative_int_max'))

    def test_float32(self):
        self.assertEqual(3.141592, self.parse('float32'))

    def test_float64(self):
        self.assertEqual(2.71828182845904, self.parse('float64'))

    def tz(self):
        return timezone.utc

    def test_past_date(self):
        expected = datetime(2000, 12, 31, 23, 58, 20, tzinfo=self.tz())
        actual = self.parse('date_past').to_datetime()
        self.assertEqual(expected, actual)

    def test_recent_date(self):
        expected = datetime(2017, 2, 11, 3, 36, 35, 382174, tzinfo=self.tz())
        actual = self.parse('date_recent').to_datetime()
        self.assertEqual(expected, actual)

    def test_future_date(self):
        expected = datetime(2047, 2, 4, 3, 37, 35, 101460, tzinfo=self.tz())
        actual = self.parse('date_future').to_datetime()
        self.assertEqual(expected, actual)

    def test_short_data(self):
        self.assertEqual(b'hi', self.parse('data_short'))

    def test_long_data(self):
        expected = b'a relatively long string that is more ' \
                   b'than 32 characters long'
        self.assertEqual(expected, self.parse('data_long'))

    def test_emtpy_ascii_string(self):
        self.assertEqual("", self.parse('ascii_string_empty'))

    def test_short_ascii_string(self):
        self.assertEqual("yo", self.parse('ascii_string_short'))

    def test_long_ascii_string(self):
        expected = 'a relatively long string with ascii characters in it'
        self.assertEqual(expected, self.parse('ascii_string_long'))

    def test_emtpy_utf16_string(self):
        self.assertEqual("", self.parse('utf16_string_empty'))

    def test_short_utf16_string(self):
        self.assertEqual("☃", self.parse('utf16_string_short'))

    def test_long_utf16_string(self):
        expected = "☃❤✓☀★☂♞☯☢☎❄♫" * 4
        self.assertEqual(expected, self.parse('utf16_string_long'))

    def test_emtpy_array(self):
        self.assertListEqual([], self.parse('array_empty'))

    def test_small_array(self):
        expected = [1, True, 3.14, 'four']
        self.assertListEqual(expected, self.parse('array_short'))

    def test_big_array(self):
        expected = [
            True, False,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
            -1, -2, -3.14,
            'four', 'five', 'six', 'seven',
            [8], [[9]],
            'ten',
            11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
            21.22,
            23, 24, 25,
            True]
        self.assertListEqual(expected, self.parse('array_long'))

    def test_emtpy_dict(self):
        self.assertDictEqual({}, self.parse('dict_empty'))

    def test_small_dict(self):
        expected = {'a': 1, 'b': 2}
        self.assertDictEqual(expected, self.parse('dict_small'))

    def test_big_dict(self):
        expected = {
            'a': 1,  'b': 2,  'c': 3,  'd': 4,  'e': 5,  'f': 6,
            'g': 7,  'h': 8,  'i': 9,  'j': 10, 'k': 11, 'l': 12,
            'm': 13, 'n': 14, 'o': 15, 'p': 16, 'q': 17, 'r': 18,
            's': 19, 't': 20, 'u': 21, 'v': 22, 'w': 23, 'x': 24,
            'y': 25, 'z': 26, '0': 27, '1': 28, '2': 29, '3': 30,
            '4': 31, '5': 32, '6': 33, '7': 34, '8': 35, '9': 36
            }
        self.assertDictEqual(expected, self.parse('dict_big'))

    def test_nested_dict(self):
        expected = { 'outer': { 'middle': { 'inner': 'value' } } }
        self.assertDictEqual(expected, self.parse('dict_nested'))


# We are testing to see if plistlib can parse our generated stuff back
# to the original objects
class TestBPlistGeneration(BPListTest):

    def test_generates_equivalent_accessibility_info(self):
        dict = plistlib.loads(self.fixture('AccessibilityDefinitions'))
        self.assertDictEqual(dict, bplist.parse(bplist.generate(dict)))

    def compare(self, object):
        self.assertEqual(object, plistlib.loads(bplist.generate(object)))

    def test_true(self):
        self.compare(True)

    def test_false(self):
        self.compare(False)

    def test_int8(self):
        self.compare(0)
        self.compare(1)
        self.compare(127)
        self.compare(128)
        self.compare(255)

    def test_int16(self):
        self.compare(256)
        self.compare(32_767)
        self.compare(32_768)
        self.compare(65_535)

    def test_int32(self):
        self.compare(65_536)
        self.compare(2_147_483_647)
        self.compare(4_294_967_295)

    def test_int64(self):
        self.compare(4_294_967_296)
        self.compare(9_223_372_036_854_775_807)

    def test_negative_int(self):
        self.compare(-1)
        self.compare(-1024)
        self.compare(-65_536)
        self.compare(-9_223_372_036_854_775_807)

    def test_float64(self):
        self.compare(cmath.e)
        self.compare(cmath.inf)

    def test_past_date(self):
        date = datetime(2000, 12, 31, 23, 58, tzinfo=timezone.utc).timestamp()
        self.compare(date)

    def test_recent_date(self):
        date = datetime(2017, 6, 23, 0, 4, tzinfo=timezone.utc).timestamp()
        self.compare(date)

    def test_future_date(self):
        date = datetime(2050, 8, 7, 6, 5, tzinfo=timezone.utc).timestamp()
        self.compare(date)

    def test_short_data(self):
        self.compare(b'')
        self.compare(b'hi')

    def test_long_data(self):
        self.compare(b'a relatively long string that is more ' \
                     b'than 32 characters long')

    def test_emtpy_ascii_string(self):
        self.compare('')

    def test_short_ascii_string(self):
        self.compare('yo')

    def test_long_ascii_string(self):
        self.compare('a relatively long string with ascii characters in it')

    def test_short_utf16_string(self):
        self.compare("☃")

    def test_long_utf16_string(self):
        self.compare("☃❤✓☀★☂♞☯☢☎❄♫" * 4)

    def test_emtpy_array(self):
        self.compare([])

    def test_small_array(self):
        self.compare([1, True, 3.14, 'four'])

    def test_big_array(self):
        array = [
            True, False,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
            -1, -2, -3.14,
            'four', 'five', 'six', 'seven',
            [8], [[9]],
            'ten',
            11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
            21.22,
            23, 24, 25,
            True]
        self.compare(array)

    def test_emtpy_dict(self):
        self.compare({})

    def test_small_dict(self):
        self.compare({'a': 1, 'b': 2})

    def test_big_dict(self):
        big_dict = {
            'a': 1,  'b': 2,  'c': 3,  'd': 4,  'e': 5,  'f': 6,
            'g': 7,  'h': 8,  'i': 9,  'j': 10, 'k': 11, 'l': 12,
            'm': 13, 'n': 14, 'o': 15, 'p': 16, 'q': 17, 'r': 18,
            's': 19, 't': 20, 'u': 21, 'v': 22, 'w': 23, 'x': 24,
            'y': 25, 'z': 26, '0': 27, '1': 28, '2': 29, '3': 30,
            '4': 31, '5': 32, '6': 33, '7': 34, '8': 35, '9': 36
            }
        self.compare(big_dict)

    def test_nested_dict(self):
        self.compare({ 'outer': { 'middle': { 'inner': 'value' } } })


class TestBPlistArchiverSupport(BPListTest):

    def generate_and_parse(self, obj):
        self.assertEqual(obj, bplist.parse(bplist.generate(obj)))

    def test_uid(self):
        self.generate_and_parse(uid(1))
        self.generate_and_parse(uid(255))
        self.generate_and_parse(uid(256))
        self.generate_and_parse(uid(32_767))
        self.generate_and_parse(uid(65_535))
        self.generate_and_parse(uid(4_000_000_000))

    def test_parse_unknown(self):
        with self.assertRaisesRegex(TypeError, "expected bytes, module found"):
            bplist.parse(bplist)

    def test_generate_unknown(self):
        with self.assertRaisesRegex(RuntimeError, "does not support"):
            bplist.generate(bplist)

if __name__ == '__main__':
    from unittest import main
    main()
