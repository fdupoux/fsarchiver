+++
weight = 2100
title = "Internals: multi-threading"
nameInMenu = "Multi-threading"
draft = false
+++

## About multi-threading
Today all the new processors are dual-core or quad-core. But all the 
standard compression tools are single threaded. It means that when
you use "tar cfz" for "tar cfj", to compress a tarball using gzip
or bzip2, you just use one cpu, so just 50% of 25% of the power you
have. FSArchiver is multi-threaded if you use option "-j" to create
several compression jobs, so that it can use all the power of your
processors to compress faster. For instance, compressing on an intel
Q6600 quad-core with bzip2 is really fast, and with lzma it's not too
slow.

## Implementation of the multi-threading
FSArchiver is using three kinds of threads even if you don't use the
option "-j". There is a main thread, a archive-io thread, and one or
more compression/decompression threads. When you use option "-j" you
just create more than one compression/decompression threads.

To implement multi-threading, fsarchiver is using the pthread library
(POSIX Pthread libraries). This is a standard threads implementation
available on many operating systems.

The most important thing in the multi-threading implementation is the
queue which is implemented in queue.c. All the data that are read of
written in a file first go in the queue. The queue is a list of items
that have to be written: it can be either an header or a data block.
The contents are split into blocks of few hundreds kilo-bytes. For
instance, if you want to archive a 10MB file, it can be spitted into
50 datablocks. The queue must be big enough to contain multiple data
blocks at a time. The compression/decompression threads are always
searching for the first block to be processed in the queue, they
process it, and update the block in the queue. For instance if the
queue is able to store 10 data blocks at a given time, it means that
a quad-core processor will have enough blocks to feed each of its 
cores, and then to use all the power of this processor. The size of 
the queue is defined by FSA_MAX_QUEUESIZE. It says how many data 
blocks can be stored in the queue at a given time. When this limit
is reached, the thread which fills the queue will have to wait.

## Overview of the threads
Here are how the threads work:

* when we write an archive (savefs / savedir):
  * the mainthread (create.c) is writing items to the queue
  * the compression thread is reading and writing in the queue
  * the archio thread is reading items to the disk (queue writer)
* when we read an archive (restfs / restrdir / archinfo):
  * the mainthread (extract.c) is reading items from the queue
  * the decompression thread is reading and writing in the queue
  * the archio thread is writing items to the disk (queue reader)

## Queue and synchronization
The queue is what links all the threads together. It's a critical
section of the code so it's very important that it contains no bug.
The consistency of this queue is guaranteed with a mutex (to make 
sure that two threads can't change the same thing at the same time).
It's very important that each function that locks this mutex unlocks
it before it exits, else there will be a dead-lock. It's also useful
to keep the queue management quite simple in order to avoid bugs.

To synchronize threads, there are two attributes:

* end_of_archive: which is an attribute of the queue
* g_stopfillqueue which is a global variable outside of the queue
* g_abort is set when the user wants to stop. It's outside of the queue

The threads have to play with these two attributes to manage events
such as errors of the end of data.

In a normal situation:

* the queue writer puts data in the queue and sets end_of_archive to true when there are no more data to be written to the queue
* the queue reader has to read data from the queue until end_of_archive is set to true and the queue is empty.

In case of errors:

* when the user press Ctrl+C, the signal handler will set g_abort to true and the main thread has to manage that case.

* if the queue writer has a problem and wants to stop, it just has to set end_of_archive to true and the queue reader will stop reading it
 queue_set_end_of_queue(&g_queue, true);

* if the queue reader has a problem and wants to stop, it has to set  g_stopfillqueue to true, to say to the queue writer that it has to stop. Then the queue writer checks g_stopfillqueue and stop filling the queue. Then the queue reader has to remove the remaining items from the queue, using queue_destroy_first_item()
 set_stopfillqueue(); // don't need any more data
   while (queue_get_end_of_queue(&g_queue)==false)
      queue_destroy_first_item(&g_queue);

## General rules for multi-threading

* all the important decisions (aborting, creating/destroying threads, ...) are
taken in the main thread (implemented in either create.c or extract.c)
* when there is an error, the first thing to do is to stop the source that fills
the queue. If you are in the thread that reads the queue, you set
g_stopfillqueue to true to stop the source of data and then process the
remaining items which are still in the queue
* in case of an error, a secondary thread has to make sure that the main thread
is aware of that error (through a global variable or through the
set_end_of_queue attribute if it's the queue writer)
* in case of an error, always make sure the queue is empty before the current
thread exits, so that the threads that are involved in the queue don't continue
to wait for data.
* the compression/decompression thread is in the middle of the chain. It exits
when queue_get_end_of_queue(&g_queue)==false, so we must be sure that the queue
is empty when we terminate with an error, else the compression thread will never
exit and the program will hang.
* only the main thread is involved in the management of the signals (when the
users does Ctrl+C), the main threads must often checks that g_abort is set to
false, and in case it's set to true, it must stops its own processing, and warn
the secondary threads that they have to terminate.
