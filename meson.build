project(
    'buse-cpp',
    'cpp',
    version: '1.0.0',
    default_options: ['warning_level=3', 'werror=false', 'cpp_std=c++23'],
)

executable(
    'ramdisk-example',
    files(
        'src/block-operator.cpp',
        'src/buse.cpp',
        'tests/ramdisk.cpp',
    ),
)

executable(
    'block-operator-test',
    files(
        'src/block-operator.cpp',
        'tests/block-operator-test.cpp',
    ),
)
