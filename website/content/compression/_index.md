+++
weight = 900
title = "Compression"
nameInMenu = "Compression"
draft = false
+++

## About
Different compression algorithms are supported:

* lzo: very fast compression but it does not compress well. You can use it
if you have a very slow cpu. You should consider lz4 if you want fast decompression.
* lz4: very fast compression but it does not compress well. You can use it
if you have a very slow cpu. It is similar to lzo but provides better performance
for decompression hence it is recommended over lzo.
* gzip: most common compression algorithm. It is quite fast and the compression
ratio is good.
* bzip2: quite slow compression algorithm, but it has a very good compression ratio.
* xz/lzma: quite recent algorithm. It has an excellent compression ratio but it is
very slow to compress. Compared to bzip2, it is much faster to decompress.
* zstd: provides better ratio/speed than other algorithms for most situations
(from good speed to good ratio). Hence it is the recommended compression method
in recent fsarchiver versions

Each algorithm provides several levels of speed/efficiency. The compression
algorithm you will use depends on how fast your processor is, how much disk
space you have, and how big the archive can be. By default, fsarchiver is
compressing using gzip with its sixth level of compression.

## Installation
All these compression algorithms are implemented in libraries that fsarchiver is
using. It means you need these libraries to be installed on your computer to
compile fsarchiver with the support for these compression algorithms. gzip,
bzip2 and xz are very common so it must not be a problem. lzo is not always
installed so you may have to install it, or to disable support for it.

## Multi-threading
fsarchiver is able to do multi-threading. Unlike many compression programs that
can use only one cpu core, fsarchiver can use all the power of your system. It
means that it can compress about four times faster on a computer with a quad-core
processor for instance.

By default, fsarchiver just creates one compression threads, so it just uses one
processor core. To enable the multi-threading compression/decompression, you have
to run fsarchiver with option **-j X**, where **X** is the number
of compression threads you want. In general, it's good to have as many
compression jobs as there are processors/cores available, except if you want to
leave enough power for other tasks. If you have a processor with multiple cores,
you can combine the multi-threading compression with a very high compression
level. That way you will have a very good compression ratio and it should not
take too much time to compress. Keep in mind that you can use the
multi-threading option at compression as well as decompression, even if it's
more interesting at compression which needs more power.

## Compression levels options
There are two options which you can use to choose the compression level you want
to use. The legacy option is "-z" (lowercase) and it provides 10 compression
levels with various speeds and ratios. These 10 levels correspond to five
compression algorithms (lz4, lzo, gzip, bzip2, xz). The new option is "-Z"
(uppercase) and it provides 22 compression levels which are all implemented by
zstd. This algorithm provides better combinations of speed/ratio than other
algorithms in most cases. Hence it is recommended to switch to this new
compression option.

## Legacy compression levels
FSArchiver provides ten legacy compression levels. You can choose the
compression level to use when you create an archive (by doing a **savefs** or
**savedir**). You just have to use option **-z X** where X is the level to
use. When you use a low number, the compression will be very quick and less
efficient. The memory requirement for compressing and decompressing will be
small. The higher the compression level is, the better the compression will be
and the smaller the archive will be. But good compression levels will require a
lot of time, and also the memory requirement can be very big.

| **Level** | **Equivalent** |
|:---------:|:--------------:|
| 0         | lz4            |
| 1         | lzo -3         |
| 2         | gzip -3        |
| 3         | gzip -6        |
| 4         | gzip -9        |
| 5         | bzip2 -2       |
| 6         | bzip2 -5       |
| 7         | lzma -1        |
| 8         | lzma -6        |
| 9         | lzma -9        |

## New compression levels
FSArchiver 0.8.4 introduced support for zstd compression. This algorithm
provides better combinations of speed/ratio than legacy algorithms in most
cases. It is almost as fast as lz4 with level 1 and it its ratio is almost as
good as xz with level 22. Compression methods in the medium of the range (gzip
and bzip2) have become irrelevant as zstd can provide results which are both
better and faster.

You can choose the compression level when you create an archive (by doing a
**savefs** or **savedir**). You just have to use option **-Z X** where X is the
level to use. When you use a low number, the compression will be very quick and
less efficient. The memory requirement for compressing and decompressing will be
small. The higher the compression level is, the better the compression will be
and the smaller the archive will be.

## High compression levels
xz and zstd provide the best compression ratios, hence a smaller archive file.
The xz decompression is faster than bzip2, and the zstd decompression is even
faster than xz for similar ratios. Hence it is recommended to use zstd with a
high compression level if you want to minimize the size of archive files. The
ratio and compression speed is similar to xz, and the zstd decompression will
be much faster than xz. The bzip2 algorithm has become irrelevant as both xz and
zstd provide better combinations of ratio/speed for high compression levels.

## Memory requirement
You must be aware that high xz and zstd compression levels require a lot of memory
especially at compression time. These compression levels are recommended on
recent computers having multiple cpu cores and large amounts of memory. If the
compression fails because lack of memory, the uncompressed version of the data
will be written to the archive and an error message will be printed in the terminal
(the archive will still be valid as long as fsarchiver continues to run). In that
case, using a lower compression level is recommended since it's likely to work.

If you use multi-threading, there will be several compression-threads running in
the same time, each one using some memory. Multi-threading compression will be
faster on multi-core processors or systems with more than one cpu in general, but
the compression ratio is the same.

In our tests, the same fsarchiver savefs command with two threads and compression
level -z9 is using 1438 MiB of memory instead of 754 MiB when it has only one
compression thread. This is because each compression thread requires a large amount
of memory when the highest compression level is used (-z9). Usually memory will not
be an issue with any recent desktop or server machine if you use compression levels
inferior to -z9.

You can read the following [
topic about memory problems](http://forums.fsarchiver.org/viewtopic.php?p=2259).

The biggest part of the memory requirement is the compression threads. The more
compression threads you have, the more memory you need. Very high compression
levels (especially -z9) requires a huge amount of memory. If you don't have
enough memory, use -z8 rather than -z9 or disable multi-threading.
