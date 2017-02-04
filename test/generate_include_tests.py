#!/usr/bin/python

import os
from glob import glob

_FORMAT_STRING = '''#include <{0}>

int main() {{ return 0; }}'''

_CMAKE_TEMPLATE = 'add_executable({0} {1}.cpp)\n'

def main():
    root = os.path.realpath(os.path.dirname(__file__))
    include_dir = os.path.join(root, '../include')
    headers = [y.replace(include_dir + '/', '') for x in os.walk(include_dir) for y in glob(os.path.join(x[0], '*.hpp'))]
    cmake_data = ''
    for header in headers:
        print header
        test_dir = os.path.join(root, 'include')
        path = os.path.join(test_dir, header)
        without_ext = os.path.splitext(path)[0]
        if not os.path.exists(os.path.dirname(path)):
            os.makedirs(os.path.dirname(path))
        if not os.path.exists(without_ext + '.cpp'):
            open(without_ext + '.cpp', 'w').write(_FORMAT_STRING.format(header))
        cmake_data += _CMAKE_TEMPLATE.format(os.path.splitext(header)[0].replace('/', '_'), os.path.relpath(without_ext, test_dir))
    open(os.path.join(test_dir, 'CMakeLists.txt'), 'w').write(cmake_data)

if __name__ == '__main__':
    main()