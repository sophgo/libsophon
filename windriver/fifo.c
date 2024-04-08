#include "bm_common.h"
#include "fifo.tmh"

#define is_power_of_2(x) ((x) != 0 && (((x) & ((x)-1)) == 0))
#define min_fifo(x, y)        (x <= y ? x : y)

int kfifo_alloc(struct kfifo *fifo, unsigned int size) {
    unsigned char *buffer;
    unsigned char *buffer_fifo;
    /*
     * round up to the next power of 2, since our 'let the indices
     * wrap' tachnique works only in this case.
     */
    if (!is_power_of_2(size)) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "size is not power of 2\n");
        return -1;
    }

    PHYSICAL_ADDRESS pa;
    pa.QuadPart     = 0xFFFFFFFFFFFFFFFF;
    buffer      = MmAllocateContiguousMemory(size, pa);
    if (!buffer)
        return -1;

    buffer_fifo = MmAllocateContiguousMemory(sizeof(struct kfifo), pa);
    if (!buffer_fifo){
        MmFreeContiguousMemory(buffer);
        return -1;
    }

    fifo->buffer = buffer;
    fifo->size   = size;
    fifo->in = fifo->out = 0;

    return STATUS_SUCCESS;
}

void kfifo_free(struct kfifo *fifo) {
    if (!fifo) {
        if (!fifo->buffer)
            MmFreeContiguousMemory(fifo->buffer);

        MmFreeContiguousMemory(fifo);
    }
}

unsigned int kfifo_len(struct kfifo *fifo) {
    return fifo->in - fifo->out;
}

unsigned int kfifo_avail(struct kfifo *fifo) {
    return fifo->size - fifo->in + fifo->out;
}
unsigned int kfifo_is_empty(struct kfifo *fifo) {//empty return 1 ,else return 0
    return (fifo->in == fifo->out);
}
unsigned int kfifo_is_full(struct kfifo *fifo)
{
    return (kfifo_len(fifo) == fifo->size);
}

unsigned int kfifo_in(struct kfifo * fifo,
                          unsigned char * from,
                         unsigned int   len) {
    unsigned int l;

    len = min_fifo(len, fifo->size - fifo->in + fifo->out);

    l = min_fifo(len, fifo->size - (fifo->in & (fifo->size - 1)));
    memcpy(fifo->buffer + (fifo->in & (fifo->size - 1)), from, l);
    memcpy(fifo->buffer, from + l, len - l);

    fifo->in += len;
    return len;
}

unsigned int kfifo_out(struct kfifo * fifo,
                         unsigned char *to,
                         unsigned int   len) {
    unsigned int l;
    len = min_fifo(len, fifo->in - fifo->out);

    l = min_fifo(len, fifo->size - (fifo->out & (fifo->size - 1)));
    memcpy(to, fifo->buffer + (fifo->out & (fifo->size - 1)), l);
    memcpy(to + l, fifo->buffer, len - l);
    fifo->out += len;

    if (fifo->in == fifo->out)//fifo is empty
        fifo->in = fifo->out = 0;

    return len;
}

unsigned int kfifo_out_peek(struct kfifo * fifo,
                            unsigned char *to,
                            unsigned int   len) {//return fifo data to buffer to, but pointer out don't move
    unsigned int l;
    len = min_fifo(len, fifo->in - fifo->out);

    l = min_fifo(len, fifo->size - (fifo->out & (fifo->size - 1)));
    memcpy(to, fifo->buffer + (fifo->out & (fifo->size - 1)), l);
    memcpy(to + l, fifo->buffer, len - l);

    return len;
}

void kfifo_reset(struct kfifo *fifo) {
    fifo->in = fifo->out = 0;
    return;
}
