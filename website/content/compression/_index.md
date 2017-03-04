+++
weight = 900
title = "Compression"
nameInMenu = "Compression"
draft = false
+++

## About
Recent fsarchiver versions comes with support for four different compression
algorithms:

* lzo: it is very fast compression but it does not compress well. You can use it
if you have a very slow cpu
* gzip: it is the most common compression algorithm. It's quite fast and the
compression ratio is good.
* bzip2: it is a quite slow compression algorithm, but it has a very good
compression ratio.
* xz/lzma: it is a quite recent algorithm. It has an excellent compression ratio
but it is very slow to compress and quite fast to decompress.

Each algorithm provides several levels of speed/efficiency. The compression
algorithm you will use depends on how fast your processor is, how much disk
space you have, and how big the archive can be. By default, fsarchiver is
compressing using gzip with its sixth level of compression.

## Installation
All these compression algorithms are implemented in libraries that fsarchiver is
using. It means you need these libraries to be installed on your computer to
compile fsarchiver with the support for these compression algorithms. gzip,
bzip2 and xz are very common so it must not be a problem. lzo is not always
installed so you may have to install it, or to disable the support for lzo
compression.

## Multi-threading
fsarchiver is able to do multi-threading. Unlike many compression programs that
can use only one cpu to compress, fsarchiver can use all the power of your cpus
if you have a a cpu with multiple cores (dual-core, quad-core) or more that one
cpu. It means that it can compress about four times faster on a computer with a
quad-core processor for instance. 

By default, fsarchiver just creates one compression threads, so it just uses one
processor. To enable the multi-threading compression/decompression, you have to
run fsarchiver with option **-j X**, where **X** is the number
of compression threads you want. In general, it's good to have as many
compression jobs as there are processors/cores available, except if you want to
leave enough power for other tasks. If you have a processor with multiple cores,
you can combine the multi-threading compression with a very high compression
level. That way you will have a very good compression ratio and it should not
take too much time to compress. Keep in mind that you can use the
multi-threading option at compression as well as decompression, even if it's
more interesting at compression which needs more power.

## Compression levels available
FSArchiver provides nine different compression levels. You can choose the
compression level to use when you create an archive (by doing a **savefs** or
**savedir**). You just have to use option **-z X** where X is the level to
use. when you use a low number, the compression will be very quick and less
efficient. The memory requirement for compressing and decompressing will be
small. The higher the compression level is, the better the compression will be
and the smaller the archive will be. But good compression levels will require a
lot of time, and also the memory requirement can be very big.

| **Level** | **Equivalent** |
|:---------:|:--------------:|
| 1         | lzo -3         |
| 2         | gzip -3        |
| 3         | gzip -6        |
| 4         | gzip -9        |
| 5         | bzip2 -2       |
| 6         | bzip2 -5       |
| 7         | lzma -1        |
| 8         | lzma -6        |
| 9         | lzma -9        |

## High compression levels
bzip2 and lzma are the best compression algorithms available with fsarchiver.
In general, bzip2 is 15% better than gzip, and lzma is 15% better than bzip2.
Better means that the archive is 15% smaller in these examples. 

Lzma has another interesting feature: its decompression is very fast, about
three times faster than bzip2, even if its compression is better. So if you
accept to spend more time at compression, the medium lzma will provide a file
which is smaller than what an average bzip2 could do, and it will decompress
faster. It's very interesting if you want to create an archive just once, and
to extract it several times (ex: software deployment).

If you don't want the compression to be too slow, you can also use the fastest
lzma option, which will be just as good as an average bzip2, and the compression
will take the same time, but it will be a lot faster to decompress than bzip2.
So the fastest lzma option is often a better choice than an average/good bzip2.

## Memory requirement
You must be aware that high lzma compression levels require a lot of memory
especially at compression time. These levels of compression are recommended on
recent computers having multiple-cores (Dual-core and Quad-core cpus) and few
GB of memory. If the compression fails because of a lack of memory, the
uncompressed version of the data will be written to the archive and an error
message will be printed in the terminal (but the archive will still be valid as
long as fsarchiver continues to run). In that case, using a lower compression
level is recommended since it's likely to work. 

You can read the following [
topic about memory problems](http://forums.fsarchiver.org/viewtopic.php?p=2259).
You can see that there is a huge difference between a typical savefs command
using -z8 (172100 KiB are used) and -z9 (754076 KiB are used).

If you use multi-threading, there will be several compression-threads running in
the same time, each one is using some memory. Multi-threading compression will
be faster or multi-core processors or systems with more than one cpu in general,
but the compression ratio is the same.

In our tests, the same fsarchiver command with two threads and compression level
z9 is using 1438MB of memory instead of 754MB when it has only one compression
thread. This is because each compression thread requires a large amount of
memory when the highest compression level is used (-z9). You can have many
compression threads if you don't use the maximum compression level, the amount
of memory required will be normal.

The biggest part of the memory requirement is the compression threads. The more
compression threads you have, the more memory you need. Very high compression
levels (especially -z9) requires a huge amount of memory. If you don't have
enough memory, use -z8 rather than -z9 or disable the multi-threading if you
have time.
