from setuptools import Extension, setup

ext = Extension(
    name='toys',
    sources=['toys.cpp', 'bullet.cpp'],
    define_macros=[('BT_USE_DOUBLE_PRECISION', None)],
    include_dirs=['bullet'],
)

setup(name='toys', version='0.1.0', ext_modules=[ext])
