bmcv_stft
============


**Processor model support**

This interface supports BM1684/BM1684X.


**Interface form:**

    .. code-block:: c

        bm_status_t bmcv_stft(
                    bm_handle_t handle,
                    float* XRHost,
                    float* XIHost,
                    float* YRHost,
                    float* YIHost,
                    int batch,
                    int L,
                    bool realInput,
                    int pad_mode,
                    int n_fft,
                    int win_mode,
                    int hop_len,
                    bool normalize);


**Parameter description:**

* bm_handle_t handle

  Input parameters. The handle of bm_handle

* float\* XRHost

  Input parameter. The device memory space storing the real number of the input data is batch * L * sizeof (float) for one-dimensional STFT.

* float\* XIHost

  Input parameter. The device memory space storing the imaginary number of the input data is batch * L * sizeof (float) for one-dimensional STFT.

* float\* YRHost

  Output parameter. The device memory space storing the real number of the output data is (n_fft / 2 + 1) * (1 + L / hop_len) * sizeof (float) for one-dimensional STFT.

* float\* YIHost

  Output parameter. The device memory space storing the imaginary number of the output data is (n_fft / 2 + 1) * (1 + L / hop_len) * sizeof (float) for one-dimensional STFT.

* int batch

  Input parameter. Number of batches.

* int L

  Input parameters. The length of each batch.

* bool realInput

  Input parameters. Whether the input is a real number, false is a complex number, true is a real number.

* int pad_mode

  Input parameters. Signal filling mode, 0 is constant, 1 is reflect.

* int n_fft

  Input parameters. The length of the FFT for each L signal length.

* int win_mode

  Input parameters. Signal windowing mode, 0 is hanning window, 1 is hamming window.

* int hop_len

  Input parameters. The sliding distance of the signal for FFT is generally n_fft / 4 or n_fft / 2.

* bool normalize

  Input parameters. Whether the output results should be normalized.


**Return value description:**

* BM_SUCCESS: success

* Other: failed


**Note:**

This interface only handles the STFT transformation of one-dimensional signals.


**Sample code:**

    .. code-block:: c

        #include "bmcv_api_ext.h"
        #include <stdint.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <math.h>
        #include <string.h>

        enum Pad_mode {
            CONSTANT = 0,
            REFLECT = 1,
        };

        enum Win_mode {
            HANN = 0,
            HAMM = 1,
        };

        int main()
        {
            bm_handle_t handle;
            int i;
            int L = 4096;
            int batch = 1;
            bool realInput = true;
            int pad_mode = REFLECT;
            int win_mode = HANN;
            int n_fft = 4096;
            int hop_length = 1024;
            bool norm = true;
            float* XRHost = (float*)malloc(L * batch * sizeof(float));
            float* XIHost = (float*)malloc(L * batch * sizeof(float));
            int num_frames = 1 + L / hop_length;
            int row_num = n_fft / 2 + 1;
            float* YRHost_tpu = (float*)malloc(batch * row_num * num_frames * sizeof(float));
            float* YIHost_tpu = (float*)malloc(batch * row_num * num_frames * sizeof(float));

            bm_dev_request(&handle, 0);
            memset(XRHost, 0, L * batch * sizeof(float));
            memset(XIHost, 0, L * batch * sizeof(float));

            for (i = 0; i < L * batch; i++) {
                XRHost[i] = (float)rand() / RAND_MAX;;
                XIHost[i] = realInput ? 0 : ((float)rand() / RAND_MAX);
            }

            bmcv_stft(handle, XRHost, XIHost, YRHost_tpu, YIHost_tpu, batch, L,
                    realInput, pad_mode, n_fft, win_mode, hop_length, norm);

            free(XRHost);
            free(XIHost);
            free(YRHost_tpu);
            free(YIHost_tpu);
            bm_dev_free(handle);
            return 0;
        }