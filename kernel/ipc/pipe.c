// SPDX-License-Identifier: GPL-3.0-or-later
// abstract pipe implementation
// A pipe is a buffer that only has a single reader and a single writer

#define pr_fmt(fmt) "pipe: " fmt

#include "mos/ipc/pipe.h"

#include "mos/mm/slab_autoinit.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/signal.h"
#include "mos/tasks/wait.h"

#include <mos/lib/sync/spinlock.h>
#include <mos_stdlib.h>

#define PIPE_MAGIC MOS_FOURCC('P', 'I', 'P', 'E')

static slab_t *pipe_slab = NULL;
SLAB_AUTOINIT("pipe", pipe_slab, pipe_t);

#define advance_buffer(buffer, bytes) ((buffer) = (void *) ((char *) (buffer) + (bytes)))

static size_t pipe_io_write(io_t *io, const void *buf, size_t size)
{
    pipe_t *pipe = container_of(io, pipe_t, writer);
    if (pipe->magic != PIPE_MAGIC)
    {
        pr_warn("pipe_io_write: invalid magic");
        return 0;
    }

    pr_dinfo2(pipe, "writing %zu bytes", size);

    // write data to buffer
    spinlock_acquire(&pipe->lock);

    if (pipe->closed)
    {
        pr_dinfo2(pipe, "%pt: pipe closed", (void *) current_thread);
        signal_send_to_thread(current_thread, SIGPIPE);
        spinlock_release(&pipe->lock);
        return -EPIPE; // pipe closed
    }

    size_t total_written = 0;

retry_write:;
    const size_t written = ring_buffer_pos_push_back(pipe->buffers, &pipe->buffer_pos, buf, size);
    advance_buffer(buf, written), size -= written, total_written += written;

    if (size > 0)
    {
        // buffer is full, wait for the reader to read some data
        pr_dinfo2(pipe, "%pt: pipe buffer full, waiting...", (void *) current_thread);
        spinlock_release(&pipe->lock);
        waitlist_wake(&pipe->waitlist, INT_MAX);              // wake up any readers that are waiting for data
        MOS_ASSERT(reschedule_for_waitlist(&pipe->waitlist)); // wait for the reader to read some data
        spinlock_acquire(&pipe->lock);

        // check if the pipe is still valid
        if (pipe->closed)
        {
            pr_dinfo2(pipe, "%pt: pipe closed", (void *) current_thread);
            signal_send_to_thread(current_thread, SIGPIPE);
            spinlock_release(&pipe->lock);
            return -EPIPE; // pipe closed
        }

        goto retry_write;
    }

    spinlock_release(&pipe->lock);

    // wake up any readers that are waiting for data
    waitlist_wake(&pipe->waitlist, INT_MAX);
    return total_written;
}

static size_t pipe_io_read(io_t *io, void *buf, size_t size)
{
    pipe_t *pipe = container_of(io, pipe_t, reader);
    if (pipe->magic != PIPE_MAGIC)
    {
        pr_warn("pipe_io_read: invalid magic");
        return 0;
    }

    pr_dinfo2(pipe, "reading %zu bytes", size);

    // read data from buffer
    spinlock_acquire(&pipe->lock);

    size_t total_read = 0;

retry_read:;
    const size_t read = ring_buffer_pos_pop_front(pipe->buffers, &pipe->buffer_pos, buf, size);
    advance_buffer(buf, read), size -= read, total_read += read;

    if (size > 0)
    {
        // check if the pipe is still valid
        if (pipe->closed && ring_buffer_pos_is_empty(&pipe->buffer_pos))
        {
            pr_dinfo2(pipe, "%pt: pipe closed", (void *) current_thread);
            spinlock_release(&pipe->lock);
            waitlist_wake(&pipe->waitlist, INT_MAX);
            pr_dinfo2(pipe, "read %zu bytes", total_read);
            return total_read; // EOF
        }

        // buffer is empty, wait for the writer to write some data
        pr_dinfo2(pipe, "%pt: pipe buffer empty, waiting...", (void *) current_thread);
        spinlock_release(&pipe->lock);
        waitlist_wake(&pipe->waitlist, INT_MAX);              // wake up any writers that are waiting for space in the buffer
        MOS_ASSERT(reschedule_for_waitlist(&pipe->waitlist)); // wait for the writer to write some data
        spinlock_acquire(&pipe->lock);
        goto retry_read;
    }

    spinlock_release(&pipe->lock);

    // wake up any writers that are waiting for space in the buffer
    waitlist_wake(&pipe->waitlist, INT_MAX);

    pr_dinfo2(pipe, "read %zu bytes", total_read);
    return total_read;
}

static void pipe_io_close(io_t *io)
{
    pipe_t *pipe;
    const char *type = io->flags & IO_READABLE ? "reader" : "writer";
    if (io->flags & IO_READABLE)
    {
        // the reader is closing, so the writer should be notified
        pipe = container_of(io, pipe_t, reader);
    }
    else if (io->flags & IO_WRITABLE)
    {
        // the writer is closing, so the reader should be notified
        pipe = container_of(io, pipe_t, writer);
    }
    else
    {
        pr_warn("pipe_io_close: invalid flags");
        MOS_UNREACHABLE();
    }

    if (pipe->magic != PIPE_MAGIC)
    {
        pr_warn("pipe_io_close: invalid magic");
        return;
    }

    spinlock_acquire(&pipe->lock);
    if (!pipe->closed)
    {
        pr_dinfo2(pipe, "pipe %s closing", type);
        pipe->closed = true;
        spinlock_release(&pipe->lock);

        // wake up any readers/writers that are waiting for data/space in the buffer
        waitlist_wake(&pipe->waitlist, INT_MAX);
    }
    else
    {
        // the other end of the pipe is already closed, so we can just free the pipe
        spinlock_release(&pipe->lock);

        pr_dinfo2(pipe, "pipe is already closed by the other end, '%s' closing", type);
        mm_free_pages(va_phyframe(pipe->buffers), pipe->buffer_npages);
        kfree(pipe);
    }
}

static const io_op_t pipe_io_ops = {
    .write = pipe_io_write,
    .read = pipe_io_read,
    .close = pipe_io_close,
};

pipe_t *pipe_create(size_t bufsize)
{
    bufsize = ALIGN_UP_TO_PAGE(bufsize);

    pipe_t *pipe = kmalloc(pipe_slab);
    pipe->magic = PIPE_MAGIC;
    pipe->buffer_npages = bufsize / MOS_PAGE_SIZE;
    pipe->buffers = (void *) phyframe_va(mm_get_free_pages(pipe->buffer_npages));
    waitlist_init(&pipe->waitlist);
    ring_buffer_pos_init(&pipe->buffer_pos, bufsize);
    io_init(&pipe->reader, IO_PIPE, IO_READABLE, &pipe_io_ops);
    io_init(&pipe->writer, IO_PIPE, IO_WRITABLE, &pipe_io_ops);
    return pipe;
}