bmcv_stft
============


**处理器型号支持：**

该接口支持BM1684/BM1684X。


**接口形式：**

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


**参数说明：**

* bm_handle_t handle

  输入参数。bm_handle 句柄。

* float\* XRHost

  输入参数。输入信号的实部地址，。空间大小为 batch * L。

* float\* XIHost

  输入参数。输入信号的虚部地址，空间大小为 batch * L。

* float\* YRHost

  输入参数。输出信号的实部地址，空间大小为 (n_fft / 2 + 1) * (1 + L / hop_len)。

* float\* YIHost

  输入参数。输出信号的虚部地址，空间大小为 (n_fft / 2 + 1) * (1 + L / hop_len)。

* int batch

  输入参数。batch数量。

* int L

  输入参数。每个batch的信号长度。

* bool realInput

  输入参数。输入是否为实数， false 为复数， true 为实数。

* int pad_mode

  输入参数。信号填充模式, 0 为 constant, 1 为 reflect。

* int n_fft

  输入参数。每个 L 信号长度做 FFT 的长度

* int win_mode

  输入参数。信号加窗模式, 0 为 hanning窗, 1 为 hamming窗。

* int hop_len

  输入参数。信号做 FFT 的滑动距离，一般为 n_fft / 4 或者 n_fft / 2。

* bool normalize

  输入参数。输出结果是否要进行归一化。


**返回值说明:**

* BM_SUCCESS: 成功

* 其他: 失败


**注意事项：**

1. 本接口只处理一维信号的STFT变换。


**示例代码**

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