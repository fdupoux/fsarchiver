project('fsarchiver', 'c', version : '0.8.6-git')

as_version = meson.project_version()
ver_arr = as_version.split('-')[0].split('.')

as_major_version = ver_arr[0]
as_medium_version = ver_arr[1]
as_minor_version = ver_arr[2]
as_patch_version = 0

deps = []
links = []

# Check OS Linux
assert(host_machine.system() == 'linux', 'Unsupported system type "@0@"'.format(host_machine.system()))

conf = configuration_data()
# Surround the version in quotes to make it a C string
conf.set_quoted('VERSION', as_version, description : 'Define version')
conf.set_quoted('PACKAGE_RELDATE', 'YYYY-MM-DD', description : 'Define the date of the release')
conf.set_quoted('PACKAGE_FILEFMT', 'FsArCh_002', description : 'Define the version of the file format' )
conf.set_quoted('PACKAGE_VERSION', as_version, description : 'Version number of package')
conf.set('PACKAGE_VERSION_A', as_major_version, description : 'Major version number')
conf.set('PACKAGE_VERSION_B', as_medium_version, description : 'Medium version number')
conf.set('PACKAGE_VERSION_C', as_minor_version, description : 'Minor version number')
conf.set('PACKAGE_VERSION_D', as_patch_version, description : 'Patch version number')

cc = meson.get_compiler('c')

thread_dep = dependency('threads')
deps += thread_dep

bz_lib = cc.find_library('bz2')
deps += bz_lib

zlib_dep = dependency('zlib')
deps += zlib_dep

lzma_dep = dependency('liblzma', required : false)
if lzma_dep.found()
  conf.set('OPTION_LZMA_SUPPORT', 1, description : 'Define to 1 to enable the support for lzma compression')
  deps += lzma_dep
endif

lzo_lib = cc.find_library('liblzo2', required : false)
if lzo_lib.found()
  conf.set('OPTION_LZO_SUPPORT', 1, description : 'Define to 1 to enable the support for lzo compression')
  deps += lzo_lib
endif

lz4_dep = dependency('liblz4', required : false)
if lz4_dep.found()
  conf.set('OPTION_LZ4_SUPPORT', 1, description : 'Define to 1 to enable the support for lz4 compression')
  deps += lz4_dep
endif

zstd_dep = dependency('libzstd', required : false)
if zstd_dep.found()
  conf.set('OPTION_ZSTD_SUPPORT', 1, description : 'Define to 1 to enable the support for zstd compression')
  deps += zstd_dep
endif

# check libgcrypt (required for crypto and md5)
crypto_lib = cc.find_library('gcrypt')
deps += crypto_lib

# check e2fsprogs and its libs
ext2fs_dep = dependency('ext2fs')
deps += ext2fs_dep

com_err_dep = dependency('com_err')
deps += com_err_dep

#e2p_lib = cc.find_library('e2p')
e2p_dep = dependency('e2p')
deps += e2p_dep

blkid_dep = dependency('blkid')
deps += blkid_dep

uuid_dep = dependency('uuid')
deps += uuid_dep

# Check for header files installed with a library
xattr_h = cc.has_header('sys/xattr.h')

# Check for standard header files.
check_headers = [
	'malloc.h',
	'unistd.h',
	'pthread.h',
	'sys/param.h',
	'sys/statfs.h',
	'sys/types.h',
	'sys/mount.h',
	'stdint.h',
	'endian.h',
	'stdbool.h',
	'stdlib.h',
	'stdio.h',
	'getopt.h',
	'fcntl.h',
	'time.h',
	'wordexp.h',
	'execinfo.h',
	'fnmatch.h',
]

foreach h : check_headers
   cc.has_header(h)
endforeach


# Check for library functions.
check_functions = [
	'strerror',
	'open64',
	'lstat64',
	'stat64',
	'fstatfs64',
	'fstatvfs64',
	'mempcpy',
	'lutimes',
]


foreach h : check_functions
   cc.has_function(h)
endforeach

configure_file(output : 'config.h', configuration : conf)

configuration_inc = include_directories('.')

subdir('src')

