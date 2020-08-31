#!/usr/bin/env python3

import os
import subprocess
import sys

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=''):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    user_options = build_ext.user_options + [
        ('cmake-extra-opts=', None, "Additional options passed to CMake"),
    ]

    def initialize_options(self):
        build_ext.initialize_options(self)
        self.cmake_extra_opts = None

    def finalize_options(self):
        build_ext.finalize_options(self)

    def run(self):
        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)

        self._setup(ext)
        self._build(ext)

    def _setup(self, ext):
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))

        if not extdir.endswith(os.path.sep):
            extdir += os.path.sep

        cmake_cmd = [
            'cmake',
            ext.sourcedir,
            '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={}'.format(extdir),
            '-DCMAKE_BUILD_TYPE=Release',
            '-DPYTHON_BINDINGS=ON',
            '-DPYTHON_PYPI_BUILD=ON',
            '-DPYTHON_EXECUTABLE=' + sys.executable,
            '-DEMBED_LUA=ON'
        ]

        if self.cmake_extra_opts is not None:
            cmake_cmd += self.cmake_extra_opts.split()

        subprocess.check_call(cmake_cmd, cwd=self.build_temp)

    def _build(self, ext):
        cmake_build_cmd = [
            'cmake',
            '--build', '.',
            '--config', 'Release',
            '--', '-j', str(os.cpu_count())
        ]

        subprocess.check_call(cmake_build_cmd, cwd=self.build_temp)


# check if CMake is available
try:
    subprocess.check_output(['cmake', '--version'])
except OSError:
    raise RuntimeError("cmake command must be available")


# setup
setup(
    name='pympsym',
    version="0.1",
    description="MPSoC Symmetry Reduction",
    long_description="mpsym is a C++/Lua/Python library that makes it possible to determine whether mappings of computational tasks to multiprocessor systems are equivalent by symmetry. It can also potentially be used to solve more general graph symmetry problems. To this end, mpsym makes use of a variety of algorithms from the field of computational group theory which are implemented from scratch.",
    url="https://github.com/Time0o/mpsym",
    author="Timo Nicolai",
    author_email="timonicolai@arcor.de",
    license="MIT",
    cmdclass=dict(build_ext=CMakeBuild),
    ext_modules=[CMakeExtension('mpsym')],
    zip_safe=False
)
