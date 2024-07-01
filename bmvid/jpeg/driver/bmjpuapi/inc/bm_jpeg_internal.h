#ifndef __BM_JPEG_INTERNAL_H__
#define __BM_JPEG_INTERNAL_H__

#include "bm_jpeg_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MIN_NUM_FREE_FB_REQUIRED 1
#define JPU_MEMORY_ALIGNMENT 0x10

/**************************************************/
/******* FORMAT STRUCTURES AND FUNCTIONS *******/
/**************************************************/

/* Convert image_format to (color_format, chroma_interleave, packed_format) */
void bm_jpu_convert_from_image_format(BmJpuImageFormat image_format, BmJpuColorFormat *color_format, BmJpuChromaFormat *chroma_interleave, BmJpuPackedFormat *packed_format);

/* Convert (color_format, chroma_interleave, packed_format) to image_format */
void bm_jpu_convert_to_image_format(BmJpuColorFormat color_format, BmJpuChromaFormat chroma_interleave, BmJpuPackedFormat packed_format, BmJpuImageFormat *image_format);

/**************************************************/
/******* ALLOCATOR STRUCTURES AND FUNCTIONS *******/
/**************************************************/

/* BmJpuAllocationFlags: flags for the BmJpuDMABufferAllocator's allocate vfunc */
typedef enum {
    BM_JPU_ALLOCATION_FLAG_CACHED       = 0,
    BM_JPU_ALLOCATION_FLAG_WRITECOMBINE = 1,
    BM_JPU_ALLOCATION_FLAG_UNCACHED     = 2
} BmJpuAllocationFlags;

typedef enum {
    BM_ION_FLAG_HEAP_VPP = 0,
    BM_ION_FLAG_HEAP_NPU = 1,
    BM_ION_FLAG_HEAP_VPU = 2
} BmJpuIonHeapFlags;

#define BM_JPU_ALLOCATION_FLAG_VPP_CACHED           ((BM_ION_FLAG_HEAP_VPP << 4) | BM_JPU_ALLOCATION_FLAG_CACHED)
#define BM_JPU_ALLOCATION_FLAG_JPU_CACHED           ((BM_ION_FLAG_HEAP_VPU << 4) | BM_JPU_ALLOCATION_FLAG_CACHED)
#define BM_JPU_ALLOCATION_FLAG_NPU_CACHED           ((BM_ION_FLAG_HEAP_NPU << 4) | BM_JPU_ALLOCATION_FLAG_CACHED)
#define BM_JPU_ALLOCATION_FLAG_VPP_WRITECOMBINE     ((BM_ION_FLAG_HEAP_VPP << 4) | BM_JPU_ALLOCATION_FLAG_WRITECOMBINE)
#define BM_JPU_ALLOCATION_FLAG_DEFAULT              BM_JPU_ALLOCATION_FLAG_VPP_CACHED

/* BmJpuMappingFlags: flags for the BmJpuDMABufferAllocator's map vfuncs
 * These flags can be bitwise-OR combined, although READ and WRITE cannot
 * both be set */
typedef enum {
    /* Map memory for CPU write access */
    BM_JPU_MAPPING_FLAG_WRITE   = (1UL << 0),
    /* Map memory for CPU read access */
    BM_JPU_MAPPING_FLAG_READ    = (1UL << 1)
    /* XXX: When adding extra flags here, follow the pattern: BM_JPU_MAPPING_FLAG_<NAME> = (1UL << <INDEX>) */
} BmJpuMappingFlags;


/************************************************/
/******* DECODER STRUCTURES AND FUNCTIONS *******/
/************************************************/


/* How to use the decoder (error handling omitted for clarity):
 *
 * Global initialization / shutdown is done by calling bm_jpu_dec_load() and
 * bm_jpu_dec_unload() respectively. These functions contain a reference counter,
 * so bm_jpu_dec_unload() must be called as many times as bm_jpu_dec_load() was,
 * or else it will not unload. Do not try to create a decoder before calling
 * bm_jpu_dec_load(), as this function loads the JPU firmware. Likewise, the
 * bm_jpu_dec_unload() function unloads the firmware. This firmware (un)loading
 * affects the entire process, not just the current thread.
 *
 * Typically, loading/unloading is done in two ways:
 * (1) bm_dec_jpu_load() gets called in the startup phase of the process, and
 *     bm_jpu_dec_unload() in the shutdown phase.
 * (2) bm_dec_jpu_load() gets called every time before a decoder is to be created,
 *     and bm_jpu_dec_unload() every time after a decoder was shut down.
 *
 * Both methods are fine; however, for (2), it is important to keep in mind that
 * the bm_jpu_dec_load() / bm_jpu_dec_unload() functions are *not* thread safe,
 * so surround their calls with mutex locks.
 *
 * How to create, use, and shutdown a decoder:
 *  1. Call bm_jpu_dec_get_bitstream_buffer_info(), and allocate a DMA buffer
 *     with the given size and alignment. This is the minimum required size.
 *     The buffer can be larger, but must not be smaller than the given size.
 *  2. Fill an instance of BmJpuDecOpenParams with the values specific to the
 *     input data. Check the documentation of BmJpuDecOpenParams for details
 *     about its fields.
 *  3. Call bm_jpu_dec_open(), passing in a pointer to the filled BmJpuDecOpenParams
 *     instance, the bitstream DMA buffer which was allocated in step 1, a callback
 *     of type bm_jpu_dec_new_initial_info_callback, and a user defined pointer
 *     that is passed to the callback (if not needed, just set it to NULL).
 *  4. Call bm_jpu_dec_decode(), and push data to it. Once initial information about
 *     the bitstream becomes available, the callback from step 3 is invoked.
 *  5. Inside the callback, the new initial info is available. The new_initial_info pointer
 *     is never NULL. In this callback, framebuffers are allocated and registered, as
 *     explained in the next steps. Steps 7-9 are performed inside the callback.
 *  6. (Optional) Perform the necessary size and alignment calculations by calling
 *     bm_jpu_calc_framebuffer_sizes(). Pass in either the frame width & height from
 *     BmJpuDecInitialInfo , or some explicit values that were determined externally.
 *     (The width & height do not have to be aligned; the function does this automatically.)
 *  7. Create an array of at least as many BmJpuFramebuffer instances as specified in
 *     min_num_required_framebuffers. Each instance must point to a DMA buffer that is big
 *     enough to hold a raw decoded frame. If step 7 was performed, allocating as many bytes
 *     as indicated by total_size is enough. Make sure the Y,Cb,Cr offsets in each
 *     BmJpuFramebuffer instance are valid. Using the bm_jpu_fill_framebuffer_params()
 *     convenience function for this is strongly recommended.
 *  8. Call bm_jpu_dec_register_framebuffers() and pass in the BmJpuFramebuffer array
 *     and the number of BmJpuFramebuffer instances.
 *     Note that this call does _not_ copy the framebuffer array, it just stores the pointer
 *     to it internally, so make sure the array is valid until the decoder is closed!
 *     This should be the last action in the callback.
 *  9. Continue calling bm_jpu_dec_decode(). Make sure the input data is not NULL.
 *     If the BM_JPU_DEC_OUTPUT_CODE_DECODED_FRAME_AVAILABLE flag is set in the output code,
 *     call bm_jpu_dec_get_decoded_frame() with a pointer to an BmJpuRawFrame instance.
 *     The instance will get filled by the function with information about the decoded frame.
 *     Once the decoded frame has been processed by the user, it is important to call
 *     bm_jpu_dec_mark_framebuffer_as_displayed() to let the decoder know that the
 *     framebuffer is available for storing new decoded frames again.
 *     If BM_JPU_DEC_OUTPUT_CODE_EOS is set, or if bm_jpu_dec_decode() returns a value other
 *     than BM_JPU_DEC_RETURN_CODE_OK, stop playback and close the decoder.
 * 10. In case a flush/reset is desired (typically after seeking), call bm_jpu_dec_flush().
 *     Note that any internal context/PTS/DTS values from the encoded and raw frames will be thrown
 *     away after this call; if for example the context is an index, the system that hands
 *     out the indices should be informed that any previously handed out index is now unused.
 * 11. After playback is finished, close the decoder with bm_jpu_dec_close().
 * 12. Deallocate framebuffer memory blocks and the bitstream buffer memory block.
 *
 * In situations where decoding and display of decoded frames happen in different threads, it
 * is necessary to wait until decoding is possible. bm_jpu_dec_check_if_can_decode() is used
 * for this purpose. This needs to be done in steps 5 and 10. Typically this is done by using
 * a thread condition variable. Example pseudo code:
 *
 *   mutex_lock(&mutex);
 *
 *   while (dec_initialized && !bm_jpu_dec_check_if_can_decode(decode) && !abort_waiting)
 *     condition_wait(&condition_variable, &mutex);
 *
 *   if (!abort_waiting)
 *     bm_jpu_dec_decode(decoder, encoded_frame, &output_code);
 *   ...
 *
 *   mutex_unlock(&mutex);
 *
 * (abort_waiting would be a flag that gets raised when something from the outside signals
 * that waiting and decoding needs to be shut down now, for example because the user wants
 * to close the player, or because the user pressed Ctrl+C. dec_initialized would be a flag
 * that is initially cleared, and raised in the initial info callback; it is pointless to
 * call bm_jpu_dec_check_if_can_decode() before the callback was executed.)
 *
 * If any video sequence parameters (like frame width and height) in the input data change,
 * the output code from bm_jpu_dec_decode() calls in step 10 will contain the
 * BM_JPU_DEC_OUTPUT_CODE_VIDEO_PARAMS_CHANGED flag. (This will never happen in step 5.)
 * When this occurs, decoding cannot continue, because the registered framebuffers are
 * of an incorrect size, and because the decoder's configuration is set up for the previous
 * parameters. Therefore, in this case, first, the decoder has to be drained of decoded-
 * but-not-yet-displayed frames like in step 12, then, it has to be closed, and opened
 * again. The BmJpuDecOpenParams structure that is then passed to the bm_jpu_dec_open()
 * call should have its frame_width and frame_height values set to 0 to ensure the
 * new sequence parameters are properly used. Then, the data that was fed into the
 * bm_jpu_dec_decode() call that set the BM_JPU_DEC_OUTPUT_CODE_VIDEO_PARAMS_CHANGED flag
 * has to be fed again to bm_jpu_dec_decode(). The initial info callback from
 * bm_jpu_dec_open() will again be called, and decoding continues as usual.
 *
 * It is also recommended to make sure that framebuffers and associated DMA buffers that
 * were allocated before the video sequence parameter change be deallocated in the
 * initial callback to avoid memory leaks.
 *
 * However, if the environment is a framework like GStreamer or libav/FFmpeg, it is likely
 * this will never have to be done, since these have their own parsers that detect parameter
 * changes and initiate reinitializations.
 */

/* Decoder output codes. These can be bitwise OR combined, so check
 * for their presence in the output_codes bitmask returned by
 * bm_jpu_dec_decode() by using a bitwise AND. */
typedef enum {
    /* Input data was used. If this code is present, the input data
     * that was given to the bm_jpu_dec_decode() must not be given
     * to a following bm_jpu_dec_decode() call; instead, new data
     * should be loaded. If this code is not present, then the decoder
     * didn't use it yet, so give it to the decoder again until this
     * code is set or an error is returned.
     * NOTE: this flag is obsolete. It used to mean something with the
     * fslwrapper backend; however, with the jpulib backend, it will
     * always use the input unless an error occurs or EOS is signaled
     * in drain mode. */
    BM_JPU_DEC_OUTPUT_CODE_INPUT_USED                   = (1UL << 0),
    /* EOS was reached; no more unfinished frames are queued internally.
     * This can be reached by bitstreams with no frame delay. */
    BM_JPU_DEC_OUTPUT_CODE_EOS                          = (1UL << 1),
    /* A fully decoded frame is now available, and can be retrieved
     * by calling bm_jpu_dec_get_decoded_frame(). */
    BM_JPU_DEC_OUTPUT_CODE_DECODED_FRAME_AVAILABLE      = (1UL << 2),
    /* There aren't enough free framebuffers available for decoding.
     * This usually happens when bm_jpu_dec_mark_framebuffer_as_displayed()
     * wasn't called before bm_jpu_dec_decode(), which can occur in
     * multithreaded environments. bm_jpu_dec_check_if_can_decode() is useful
     * to avoid this. Also see the guide above for more. */
    BM_JPU_DEC_OUTPUT_CODE_NOT_ENOUGH_OUTPUT_FRAMES     = (1UL << 3),
    /* Input data for a frame is incomplete. No decoded frame will
     * be available until the input frame's data has been fully and
     * correctly delivered. */
    BM_JPU_DEC_OUTPUT_CODE_NOT_ENOUGH_INPUT_DATA        = (1UL << 4),
    /* The JPU detected a change in the video sequence parameters
     * (like frame width and height). Decoding cannot continue. See the
     * explanation in the step-by-step guide above for what steps to take
     * if this output code is set. Note that this refers to detected
     * changes in the *input data*, not to the decoded frames. This means
     * that this flag is set immediately when input data with param changes
     * is fed to the decoder, even if this is for example a h.264 high
     * profile stream with lots of frame reordering and frame delays. */
    BM_JPU_DEC_OUTPUT_CODE_VIDEO_PARAMS_CHANGED         = (1UL << 5)
} BmJpuDecOutputCodes;

/* Convenience function which fills fields of the BmJpuFramebuffer structure, based on data from "calculated_sizes".
 * The specified DMA buffer and context pointer are also set. */
BmJpuDecReturnCodes bm_jpu_fill_framebuffer_params(BmJpuFramebuffer *framebuffer,
                                    BmJpuFramebufferSizes *calculated_sizes,
                                    bm_jpu_phys_addr_t dma_buffer_addr,
                                    size_t dma_buffer_size,
                                    void* context);

/* Callback for handling new BmJpuDecInitialInfo data. This is called when new
 * information about the bitstream becomes available. output_code can be useful
 * to check why this callback was invoked. BM_JPU_DEC_OUTPUT_CODE_INITIAL_INFO_AVAILABLE
 * is always set. Every time this callback gets called, new framebuffers should be
 * allocated and registered with bm_jpu_dec_register_framebuffers().
 * user_data is a user-defined pointer that is passed to this callback. It has the same
 * value as the callback_user_data pointer from the bm_jpu_dec_open() call.
 * The callback returns 0 if something failed, nonzero if successful. */
typedef BmJpuDecReturnCodes (*bm_jpu_dec_new_initial_info_callback)(BmJpuDecoder *decoder,
                                                    BmJpuDecInitialInfo *new_initial_info,
                                                    unsigned int output_code,
                                                    void *user_data);

/* Called before bm_jpu_dec_open(), it returns the alignment and size for the
 * physical memory block necessary for the decoder's bitstream buffer. The user
 * must allocate a DMA buffer of at least this size, and its physical address
 * must be aligned according to the alignment value. */
void bm_jpu_dec_get_bitstream_buffer_info(size_t *size, unsigned int *alignment);

void bm_jpu_dec_set_interrupt_timeout(BmJpuDecoder *decoder, int timeout, int count);

/* Opens a new decoder instance. "open_params", "bitstream_buffer", and "new_initial_info"
 * must not be NULL. "callback_user_data" is a user-defined pointer that is passed on to
 * the callback when it is invoked. The bitstream buffer must use the alignment and size
 * that bm_jpu_dec_get_bitstream_buffer_info() specifies (it can also be larger, but must
 * not be smaller than the size this function gives). */
BmJpuDecReturnCodes bm_jpu_dec_open(BmJpuDecoder **decoder, BmJpuDecOpenParams *open_params,
                                    bm_device_mem_t *bitstream_buffer,
                                    bm_jpu_dec_new_initial_info_callback new_initial_info_callback,
                                    void *callback_user_data);

/* Closes a decoder instance. Trying to close the same instance multiple times results in undefined behavior. */
BmJpuDecReturnCodes bm_jpu_dec_close(BmJpuDecoder *decoder);

/* Flushes the decoder. Any internal undecoded or queued frames are discarded. */
BmJpuDecReturnCodes bm_jpu_dec_flush(BmJpuDecoder *decoder);

/* Registers the specified array of framebuffers with the decoder. This must be called after
 * bm_jpu_dec_decode() returned an output code with BM_JPU_DEC_OUTPUT_CODE_INITIAL_INFO_AVAILABLE
 * set in it. Registering can happen only once during the lifetime of a decoder instance. If for some reason
 * framebuffers need to be re-registered, the instance must be closed, and a new one opened.
 * The caller must ensure that the specified framebuffer array remains valid until the decoder instance
 * is closed, since this function does not copy it; it just stores a pointer to the array internally. Also
 * note that internally, values might be written to the array (though it will never be reallocated
 * and/or freed from the inside). Also, the framebuffers' DMA buffers will be memory-mapped until the decoder
 * is closed.
 *
 * Since this function only stores a pointer to the framebuffer array internally, and does not actually copy
 * the array, it is possible - and valid - to modify the "context" fields of the framebuffers even after
 * this call was made. This is useful if for example system resources are associated later with the
 * framebuffers. In this case, it is perfectly OK to set "context" to NULL initially, and later, when the
 * resources are available, associated them to the framebuffers by setting the context fields, even if
 * bm_jpu_dec_register_framebuffers() was already called earlier.
 *
 * The framebuffers must contain valid values. The convenience functions bm_jpu_calc_framebuffer_sizes() and
 * bm_jpu_fill_framebuffer_params() can be used for this. Note that all framebuffers must have the same
 * stride values. */
BmJpuDecReturnCodes bm_jpu_dec_register_framebuffers(BmJpuDecoder *decoder, BmJpuFramebuffer *framebuffers, unsigned int num_framebuffers);

/* Decodes an encoded input frame. "encoded_frame" must always be set, even in drain mode. See BmJpuEncodedFrame
 * for details about its contents. output_code is a bit mask, must not be NULL, and returns important information
 * about the decoding process. The value is a bitwise OR combination of the codes in BmJpuDecOutputCodes. Also
 * look at bm_jpu_dec_get_decoded_frame() about how to retrieve decoded frames (if these exist). Note that if
 * the BM_JPU_DEC_OUTPUT_CODE_VIDEO_PARAMS_CHANGED flag is set in the output_code, decoding cannot continue,
 * and the decoder should be closed. See the notes below step-by-step guide above for details about this. */
BmJpuDecReturnCodes bm_jpu_dec_decode(BmJpuDecoder *decoder, bm_device_mem_t mem, BmJpuEncodedFrame const *encoded_frame, unsigned int *output_code);

/* Retrieves a decoded frame. The structure referred to by "decoded_frame" will be filled with data about
 * the decoded frame. "decoded_frame" must not be NULL.
 *
 * Calling this function before bm_jpu_dec_decode() results in an BM_JPU_DEC_RETURN_CODE_WRONG_CALL_SEQUENCE
 * return value. Calling this function more than once after a bm_jpu_dec_decode() yields the same result. */
BmJpuDecReturnCodes bm_jpu_dec_get_decoded_frame(BmJpuDecoder *decoder, BmJpuRawFrame *decoded_frame);


/* Check if the JPU can decode right now. While decoding a video stream, sometimes the JPU may not be able
 * to decode. This is directly related to the set of free framebuffers. If this function returns 0, decoding
 * should not be attempted until after bm_jpu_dec_mark_framebuffer_as_displayed() was called. If this
 * happens, bm_jpu_dec_check_if_can_decode() should be called again to check if the situation changed and
 * decoding can be done again. Also, calling this function before the initial info callback was executed is
 * not recommended and causes undefined behavior. See the explanation above for details. */
int bm_jpu_dec_check_if_can_decode(BmJpuDecoder *decoder);

/* Marks a framebuffer as displayed. This always needs to be called once the application is done with a decoded
 * frame. It returns the framebuffer to the JPU pool so it can be reused for further decoding. Not calling
 * this will eventually cause the decoder to fail, because it won't find any free framebuffer for storing
 * a decoded frame anymore.
 *
 * It is safe to mark a framebuffer multiple times. The library will simply ignore the subsequent calls. */
BmJpuDecReturnCodes bm_jpu_dec_mark_framebuffer_as_displayed(BmJpuDecoder *decoder, BmJpuFramebuffer *framebuffer);


/************************************************/
/******* ENCODER STRUCTURES AND FUNCTIONS *******/
/************************************************/


/* How to use the encoder (error handling omitted for clarity):
 *
 * Global initialization / shutdown is done by calling bm_jpu_enc_load() and
 * bm_jpu_enc_unload() respectively. These functions contain a reference counter,
 * so bm_jpu_enc_unload() must be called as many times as bm_jpu_enc_load() was,
 * or else it will not unload. Do not try to create a encoder before calling
 * bm_jpu_enc_load(), as this function loads the JPU firmware. Likewise, the
 * bm_jpu_enc_unload() function unloads the firmware. This firmware (un)loading
 * affects the entire process, not just the current thread.
 *
 * Typically, loading/unloading is done in two ways:
 * (1) bm_jpu_enc_load() gets called in the startup phase of the process, and
 *     bm_jpu_enc_unload() in the shutdown phase.
 * (2) bm_jpu_enc_load() gets called every time before a encoder is to be created,
 *     and bm_jpu_enc_unload() every time after a encoder was shut down.
 *
 * Both methods are fine; however, for (2), it is important to keep in mind that
 * the bm_jpu_enc_load() / bm_jpu_enc_unload() functions are *not* thread safe,
 * so surround their calls with mutex locks.
 *
 * How to create, use, and shutdown an encoder:
 *  1. Call bm_jpu_enc_get_bitstream_buffer_info(), and allocate a DMA buffer
 *     with the given size and alignment. This is the minimum required size.
 *     The buffer can be larger, but must not be smaller than the given size.
 *  2. Fill an instance of BmJpuEncOpenParams with the values specific to the
 *     input data. Check the documentation of BmJpuEncOpenParams for details
 *     about its fields. It is recommended to set default values by calling
 *     bm_jpu_enc_set_default_open_params() and afterwards set any explicit valus.
 *  3. Call bm_jpu_enc_open(), passing in a pointer to the filled BmJpuEncOpenParams
 *     instance, and the DMA buffer of the bitstream DMA buffer which was allocated in
 *     step 1.
 *  4. Call bm_jpu_enc_get_initial_info(). The encoder's initial info contains the
 *     minimum number of framebuffers that must be allocated and registered, and the
 *     address alignment that must be used when allocating DMA memory  for these
 *     framebuffers.
 *  5. (Optional) Perform the necessary size and alignment calculations by calling
 *     bm_jpu_calc_framebuffer_sizes(). Pass in the width & height of the frames that
 *     shall be encoded. (The width & height do not have to be aligned; the function
 *     does this automatically.)
 *  6. (Optional) allocate a DMA buffer for the input frames. Only one buffer is necessary.
 *     If the incoming data is already stored in DMA buffers, this step can be omitted,
 *     since the encoder can then read the data directly.
 *  7. Create an instance of BmJpuRawFrame, set its values to zero (typically by using memset()).
 *  8. Create an instance of BmJpuEncodedFrame. Set its values to zero (typically by using memset()).
 *  9. Set the framebuffer pointer of the BmJpuRawFrame's instance from step 7 to refer to the
 *     input DMA buffer (either the one allocated in step 6, or the one containing the input data if
 *     it already comes in DMA memory).
 * 10. Fill an instance of BmJpuEncParams with valid values. It is recommended to first set its
 *     values to zero by using memset() to set default values. It is essential to make sure the
 *     acquire_output_buffer() and finish_output_buffer() function pointers are set, as these are
 *     used for acquiring buffers to write encoded output data into.
 *     Alternatively, set write_output_data() if write-callback style output is preferred. If this
 *     function pointer is non-NULL, then acquire_output_buffer() and finish_output_buffer() are
 *     ignored.
 * 11. If step 6 was performed, and therefore input data does *not* come in DMA memory, copy the
 *     pixels from the raw input frames into the DMA buffer allocated in step 6. Otherwise, if
 *     the raw input frames are already stored in DMA memory, this step can be omitted.
 * 12. Call bm_jpu_enc_encode(). Pass the raw frame, the encoded frame, and the encoding param
 *     structures from steps 9, 10, and 12 to it.
 *     This function will encode data, and acquire an output buffer to write the encoded data into
 *     by using the acquire_output_buffer() function pointer set in step 10. Once it is done
 *     encoding, it will call the finish_output_buffer() function from step 10. Any handle created
 *     by acquire_output_buffer() will be copied over to the encoded data frame structure. When
 *     bm_jpu_enc_encode() exits, this handle can then be used to further process the output data.
 *     It is guaranteed that once acquire_output_buffer() was called, finish_output_buffer() will
 *     be called, even if an error occurred.
 *     The BM_JPU_ENC_OUTPUT_CODE_ENCODED_FRAME_AVAILABLE output code bit will always be set
 *     unless the function returned a code other than BM_JPU_ENC_RETURN_CODE_OK.
 *     If the BM_JPU_ENC_OUTPUT_CODE_CONTAINS_HEADER bit is set, then header data has been
 *     written in the output memory block allocated in step 8. It is placed right before the
 *     actual encoded frame data. bm_jpu_enc_encode() will pass over the combined size of the header
 *     and the encoded frame data to acquire_output_buffer() in this case, ensuring that the output
 *     buffers are big enough.
 *     If write-callback style output is used instead (= if the write_output_data() function pointer
 *     inside the encoding_params is set to a valid value), then this function haves as described
 *     above, except that it does not call acquire_output_buffer() or finish_output_buffer(). It
 *     still adds headers etc. but outputs these immediately by calling write_output_data().
 * 13. Repeat steps 11 to 14 until there are no more frames to encode or an error occurs.
 * 14. After encoding is finished, close the encoder with bm_jpu_enc_close().
 * 15. Deallocate framebuffer memory blocks, the input DMA buffer block, the output memory block,
 *     and the bitstream buffer memory block.
 *
 * Note that the encoder does not use any kind of frame reordering. h.264 data uses the
 * baseline profile. An input frame immediately results in an output frame (unless an error occured).
 * There is no delay.
 *
 * The JPU's encoders supports all formats from BmJpuColorFormat.
 */

/* Encoder output codes. These can be bitwise OR combined, so check
 * for their presence in the output_codes bitmask returned by
 * bm_jpu_enc_encode() by using a bitwise AND. */
typedef enum {
    /* Input data was used. If this code is present, the input frame
     * that was given to the bm_jpu_dec_encode() must not be given
     * to a following bm_jpu_dec_encode() call; instead, a new frame
     * should be loaded. If this code is not present, then the encoder
     * didn't use it yet, so give it to the encoder again until this
     * code is set or an error is returned. */
    BM_JPU_ENC_OUTPUT_CODE_INPUT_USED                 = (1UL << 0),
    /* A fully encoded frame is now available. The encoded_frame argument
     * passed to bm_jpu_enc_encode() contains information about this frame. */
    BM_JPU_ENC_OUTPUT_CODE_ENCODED_FRAME_AVAILABLE    = (1UL << 1),
    /* The data in the encoded frame also contains header information
     * like SPS/PSS for h.264. Headers are always placed at the beginning
     * of the encoded data, and this code is never present if the
     * BM_JPU_ENC_OUTPUT_CODE_ENCODED_FRAME_AVAILABLE isn't set. */
    BM_JPU_ENC_OUTPUT_CODE_CONTAINS_HEADER            = (1UL << 2)
} BmJpuEncOutputCodes;

/* Structure used together with bm_jpu_enc_open() */
typedef struct {
    /* Width and height of the incoming frames, in pixels. These
     * do not have to be aligned to any boundaries. */
    unsigned int frame_width;
    unsigned int frame_height;
    /* Color format to use for incoming frames. MJPEG actually uses
     * all possible values.
     * See the BmJpuColorFormat documentation for an explanation how
     * the chroma_interleave value can affec the pixel format that is used. */
    BmJpuColorFormat color_format;

    /* Quality factor for JPEG encoding, between 1 (worst quality, best
     * compression) and 100 (best quality, worst compression). Default
     * value is 85.
     * This quality factor is the one from the Independent JPEG Group's
     * formula for generating a scale factor out of the quality factor.
     * This means that this quality factor is exactly the same as the
     * one used by libjpeg. */
    unsigned int quality_factor;

    BmJpuChromaFormat chroma_interleave;

    BmJpuPackedFormat packed_format;
    int device_index;

    int rotationEnable;
    BmJpuRotateAngle rotationAngle;
    int mirrorEnable;
    BmJpuMirrorDirection mirrorDirection;

    int timeout;
    int timeout_count;
} BmJpuEncOpenParams;

typedef struct {
    /* Functions for acquiring and finishing output buffers. See the
     * typedef documentations above for details about how these
     * functions should behave, and the bm_jpu_enc_encode()
     * documentation for how they are used.
     * Note that these functions are only used if write_output_data
     * is set to NULL. */
    BmJpuEncAcquireOutputBuffer acquire_output_buffer;
    BmJpuEncFinishOutputBuffer finish_output_buffer;

    /* Function for directly passing the output data to the user
     * without copying it first.
     * Using this function will inhibit calls to acquire_output_buffer
     * and finish_output_buffer. See the typedef documentations
     * above for details about how this function should behave, and
     * the bm_jpu_enc_encode() documentation for how they are used.
     * Note that if this function is NULL then acquire_output_buffer
     * and finish_output_buffer must be set. */
    BmJpuWriteOutputData write_output_data;
    int bs_in_device;

    /* User supplied value that will be passed to the functions */
    void *output_buffer_context;
} BmJpuEncParams;

/* Called before bm_jpu_enc_open(), it returns the alignment and size for the
 * physical memory block necessary for the encoder's bitstream buffer. The user
 * must allocate a DMA buffer of at least this size, and its physical address
 * must be aligned according to the alignment value. */
void bm_jpu_enc_get_bitstream_buffer_info(size_t *size, unsigned int *alignment);

void bm_jpu_enc_set_interrupt_timeout(BmJpuEncoder *encoder, int timeout, int count);

/* Set the fields in "open_params" to valid defaults
 * Useful if the caller wants to modify only a few fields (or none at all) */
BmJpuEncReturnCodes bm_jpu_enc_set_default_open_params(BmJpuEncOpenParams *open_params);

/* Opens a new encoder instance. "open_params" and "bitstream_buffer" must not be NULL. */
BmJpuEncReturnCodes bm_jpu_enc_open(BmJpuEncoder **encoder, BmJpuEncOpenParams *open_params,
                                    bm_device_mem_t *bitstream_buffer);

/* Closes a encoder instance. Trying to close the same instance multiple times results in undefined behavior. */
BmJpuEncReturnCodes bm_jpu_enc_close(BmJpuEncoder *encoder);

/* Retrieves initial information available after calling bm_jpu_enc_open(). */
BmJpuEncReturnCodes bm_jpu_enc_get_initial_info(BmJpuEncoder *encoder, BmJpuEncInitialInfo *info);

/* Encodes a given raw input frame with the given encoding parameters. encoded_frame is filled with information
 * about the resulting encoded output frame. The encoded frame data itself is stored in a buffer that is
 * allocated by user-supplied functions (which are set as the acquire_output_buffer and finish_output_buffer
 * function pointers in the encoding_params).
 *
 * Encoding internally works as follows: first, the actual encoding operation is performed by the JPU. Next,
 * information about the encoded data is queried, particularly its size in bytes. Once this size is known,
 * acquire_output_buffer() from encoding_params is called. This function must acquire a buffer that can be
 * used to store the encoded data. This buffer must be at least as large as the size of the encoded data
 * (which is given to acquire_output_buffer() as an argument). The return value of acquire_output_buffer()
 * is a pointer to the (potentially memory-mapped) region of the buffer. The encoded frame data is then
 * copied to this buffer, and finish_output_buffer() is called. This function can be used to inform the
 * caller that the encoder is done with this buffer; it now contains encoded data, and will not be modified
 * further. encoded_frame is filled with information about the encoded frame data.
 * If acquiring the buffer fails, acquire_output_buffer() returns a NULL pointer.
 * NOTE: again, finish_output_buffer() is NOT a function to free the buffer; it just signals that the encoder
 * won't touch the memory inside the buffer anymore.
 *
 * acquire_output_buffer() can also pass on a handle to the acquired buffer (for example, in FFmpeg/libav,
 * this handle would be a pointer to an AVBuffer). The handle is called the "acquired_handle".
 * acquire_output_buffer() can return such a handle. This handle is copied to the encoded_frame struct's
 * acquired_handle field. This way, a more intuitive workflow can be used; if for example, acquire_output_buffer()
 * returns an AVBuffer pointer as the handle, this AVBuffer pointer ends up in the encoded_frame. Afterwards,
 * encoded_frame contains all the necessary information to process the encoded frame data.
 *
 * It is guaranteed that once the buffer was acquired, finish_output_buffer() will always be called, even if
 * an error occurs. This prevents potential memory/resource leaks if the finish_output_buffer() call somehow
 * unlocks or releases the buffer for further processing. The acquired_handle is also copied to encoded_frame
 * even if an error occurs, unless the error occurred before the acquire_output_buffer() call, in which case
 * the encoded_frame's acquired_handle field will be set to NULL.
 *
 * The aforementioned sequences involve a copy (encoded data is copied into the acquired buffer). As an
 * alternative, a write-callback-style mode of operation can be used. This alternative mode is active if
 * the write_output_data function pointer in encoding_params is not NULL. In this mode, neither
 * acquire_output_buffer() nor finish_output_buffer() are called. Instead, whenever the encoder needs to
 * write out data, it calls write_output_data().
 *
 * The other fields in encoding_params specify additional encoding parameters, which can vary from frame to
 * frame.
 * output_code is a bit mask containing information about the encoding result. The value is a bitwise OR
 * combination of the codes in BmJpuEncOutputCodes.
 *
 * None of the arguments may be NULL. */
BmJpuEncReturnCodes bm_jpu_enc_encode(BmJpuEncoder *encoder,
                                      BmJpuRawFrame const *raw_frame,
                                      BmJpuEncodedFrame *encoded_frame,
                                      BmJpuEncParams *encoding_params,
                                      unsigned int *output_code);

int bm_jpu_hwreset();

int bm_jpu_get_dump();

#ifdef __cplusplus
}
#endif

#endif /* __BM_JPEG_INTERNAL_H__ */
