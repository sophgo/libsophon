bmrt_test Use and bmodel Verification
============================================

bmrt_test tool
____________________________________________

bmrt_test is a tool for testing the correctness and actual running performance of bmodel based on the bmruntime interface. It contains the following functions:

  1. Directly inferring random data bmodel to verify the integrity and operability of bmodel;
  2. Directly using bmodel for inference through fixed input data, comparing the output and reference data and verifying the correctness of the data;
  3. Testing the actual running time of bmodel;
  4. Profiling bmodel through the bmprofile mechanism.


bmrt_test Parameter Description
_____________________________________________

   .. table:: bmrt_test main parameter description
      :widths: 15 10 50
 
      +---------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------+
      |    args       |    type    |                                                           Description                                                                       |
      +===============+============+=============================================================================================================================================+
      |  context_dir  |   string   |  The result folder compiled by the model: the comparison data is also generated during compilation and the comparison is enabled by default.|
      |               |            |                                                                                                                                             |
      |               |            |  When comparison is disabled, the folder contains a compilation.bmodel file.                                                                |
      |               |            |                                                                                                                                             |
      |               |            |  When the comparison is enabled, the folder should contain three files: compilation.bmodel,                                                 |
      |               |            |  input_ref_data.dat, output_ref_data.dat                                                                                                    |
      +---------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------+
      |   bmodel      |   string   |  Choose one from context_dir and bmodel, and specify the .bmodel file directly.Comparison is disabled by default.                           |
      +---------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------+
      |    devid      |   int      |  Optional, specifying the running device by id on multi-core platforms, with the default value being 0.                                     |
      +---------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------+
      |   compare     |   bool     |  Optional, 0 indicates comparision is disabled and 1 indicates comparison is enabled.                                                       |
      +---------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------+
      |   accuracy_f  |   float    |  Optional, specifying the float data comparison error threshold, with the default value being 0.01.                                         |
      +---------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------+
      |   accuracy_i  |   int      |  Optional, speciying the integer data comparison error threshold, with the default value being 0.                                           |
      +---------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------+
      |    shapes     |  string    |  Optional, specifying the input shapes in testing, with the compilation input shape for bmodel being used by default.                       |
      |               |            |                                                                                                                                             |
      |               |            |  The format is “[x,x,x,x],[x,x]”,                                                                                                           |
      |               |            |  corresponding to the sequence and number of inputs for the model.                                                                          |
      +---------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------+
      |   loopnum     |   int      |  Optional, specifying the times of continuous operations, with the default value being 1.                                                   |
      +---------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------+
      |  thread_num   |   int      |  Optional, specifying the number of running threads, with the default value being 1, and testing the correctness of multiple threads.       |
      +---------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------+
      |   net_idx     |   int      |  Optional, selecting the net to run by the serial No. in a bmodel that contains multiple nets.                                              |
      +---------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------+
      |  stage_idx    |   int      |  Optional, selecting the stage to run by the serial No. in a bmodel that contains multiple stages.                                          |
      +---------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------+
      |  subnet_time  |   bool     |  Optional, indicating whether to display the subnet time of bmodel.                                                                         |
      +---------------+------------+---------------------------------------------------------------------------------------------------------------------------------------------+

bmrt_test Output
____________________________________________

  .. code-block:: shell

      [BMRT][bmrt_test:981] INFO:net[vgg16_8_bmnetc] stage[0], launch total time is
      19590 us (npu 19489 us, cpu 101 us)  (1)
      [BMRT][bmrt_test:984] INFO:+++ The network[vgg16_8_bmnetc] stage[0] output_data +++
      [BMRT][print_array:647] INFO:output data #0 shape: [4 32 120 68 ] < 3.49693 4.07723 4.30039 4.14311 4.11042 4.23445 4.23644 4.23897 4.23897 4.23897 4.23897 4.23897 4.23897 4.23897 4.23897 4.23897 ... > len=1044480  (2)
      [BMRT][print_array:647] INFO:output data #1 shape: [4 32 60 34 ] < 3.523 3.94491 4.09504 4.02145 3.95682 3.96846 3.96972 3.97314 3.9728 3.9728 3.9728 3.9728 3.9728 3.9728 3.9728 3.9728 ... > len=261120
      [BMRT][print_array:647] INFO:output data #2 shape: [4 32 30 17 ] < 4.18294 5.16457 5.26347 5.16108 5.0436 4.99669 4.99279 4.99279 4.99279 4.99279 4.99279 4.99651 5.02305 5.0925 5.23303 5.24913 ... > len=65280
      [BMRT][bmrt_test:1029] INFO:load input time(s): 0.008511 (3)
      [BMRT][bmrt_test:1030] INFO:calculate  time(s): 0.019594
      [BMRT][bmrt_test:1031] INFO:get output time(s): 0.006001 (4)
      [BMRT][bmrt_test:1032] INFO:compare    time(s): 0.002886

  Main focuses:
    (1) Pure inference time of the model, excluding loading the input and getting the output.
    (2) Inference data display: The information of the successful comparison equation will be displayed if the comparison is enabled.
    (3) Use s2d to load the input data time, Usually, the pre-processing will put the data directly on the device without such time consumption.
    (4) Use d2s to take out the output data time, which usually means the data transmission time on the pcie. Mmap, with a faster speed, can be used on the SOC.

Common Methods of bmrt_test
____________________________________________

  .. code-block:: shell

      bmrt_test --context_dir bmodel_dir  # Run bmodel and compare the data.The
      # bmodel_dir should include compilation.bmodel/input_ref_data.dat/output_ref_data.dat.
      bmrt_test --context_dir bmodel_dir  --compare=0 # Run bmodel，The bmodel_dir
      # should include compilation.bmode.
      bmrt_test --bmodel xxx.bmodel # Directly run bmodel without comparing data
      bmrt_test --bmodel xxx.bmodel --stage_idx 0  --shapes "[1,3,224,224]" # Run the
      # multi-stage bmodel model and specify the bmodel for running stage 0.

      # The following instructions are functions provided by using environmental variables
      # and bmruntime and can be used by other applications.
      BMRUNTIME_ENABLE_PROFILE=1 bmrt_test --bmodel xxx.bmodel # Generate
      # profile data: bmprofile_data-x
      BMRT_SAVE_IO_TENSORS=1 bmrt_test --bmodel xxx.bmodel  # Save the
      # model inference data as input_ref_data.dat.bmrt and output_ref_data.dat.bmrt.


Comparison Data Generation and Verification Example
___________________________________________________

1. Upon the completion of model compilation, run with comparing the model. 

    When compiling the model, you must indicate \--cmp=True, which is enabled by default. input_ref_data.dat and output_ref_data.dat files will be generated in the compilation output folder.

    Then, execute 'bmrt_test \--context_dir bmodel_dir'to verify the correctness of the model inference data.

2. Comparison of pytorch original model and compiled bmodel data

    Convert the input input_data and output output_data of the pytorch model to numpy array (torch tensor can use tensor.numpy()), and then save the file (see the codes below).

    .. code-block:: python

        # Single inputs and single outputs
        input_data.astype(np.float32).tofile("input_ref_data.dat")  # astype will
        # convert according to the input data type of bmodel
        output_data.astype(np.float32).tofile("output_ref_data.dat")  # astype will
        # convert according to the output data type of bmodel

        # Multiple inputs and multiple outputs
        with open("input_ref_data.dat", "wb") as f:
            for input_data in input_data_list:
                f.write(input_data.astype(np.float32).tobytes())  # astype will convert
                # according to the input data type of bmodel
        with open("output_ref_data.dat", "wb") as f:
            for output_data in output_data_list:
                f.write(output_data.astype(np.float32).tobytes())  # astype will convert
                # according to the output data type of bmodel

    Put the generated input_ref_data.dat and output_ref_data.dat in the bmodel_dir file folder 
    and then in 'bmrt_test \--context_dir bmodel_dir' to see if the result is a comparison error.

FAQs
_________________

1. Will data comparison error occur when compiling the model?

Our bmcompiler internally uses 0.01 as the comparison threshold, which may exceed the range and report an error in a few cases.

If there is any problem with the implementation on a certain layer, there will be a piece-by-piece comparison error, and we need to give feedback to our developers.

If there are sporadic errors in random positions, it may be caused by errors in the calculation of individual values. The reason is that random data is used when compiling, which cannot be ruled out. Therefore, it is recommended to add \--cmp 0 when compiling, and verify whether the result is correct on the actual business program.

Another possibility is that there are random operators (such as uniform_random) or sorting operators (such as topk, nms, argmin, etc.) in the network, as the floating-point mantissa error of the input data will be generated in the previous calculation process, even if it is small, and will cause the difference in the indexes of sorted results. In this case, it can be seen that there is a difference in the order of the data with errors in the comparison, and it can only be tested in the actual business.