from os.path import dirname, join
from pythonbenchmark import compare, measure
from io import BytesIO
from bpylist import bplist
import plistlib

def python_serialize(data):
    plistlib.dumps(data)

def python_deserialize(obj):
    plistlib.loads(obj, fmt=plistlib.FMT_BINARY)

def c_serialize(data):
    bplist.generate(data)

def c_deserialize(obj):
    bplist.parse(obj)

fixture_path = join(dirname(__file__), 'AccessibilityDefinitions.plist')
with open(fixture_path, mode='rb') as fd:
    plist_data = fd.read()

plist_obj = plistlib.loads(plist_data)

compare(python_deserialize, c_deserialize, 128, plist_data)
compare(python_serialize, c_serialize, 128, plist_obj)
