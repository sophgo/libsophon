bmrt_test工具使用及bmodel验证
============================================

bmrt_test工具
____________________________________________

bmrt_test是基于bmruntime接口实现的对bmodel的正确性和实际运行性能的测试工具。包含以下功能:

  1. 直接用随机数据bmodel进行推理，验证bmodel的完整性及可运行性
  2. 通过固定输入数据直接用bmodel进行推理，会对输出与参考数据进行比对，验证证数据正确性
  3. 测试bmodel的实际运行时间
  4. 通过bmprofile机制进行bmodel的profile


bmrt_test参数说明
_____________________________________________

   .. table:: bmrt_test主要参数说明
      :widths: 15 10 50
 
      +---------------+------------+-----------------------------------------------------------------+
      |    args       |    type    |                   Description                                   |
      +===============+============+=================================================================+
      |  context_dir  |   string   |  模型编译出的结果文件夹，比对数据也是编译时生成的, 默认开启比对 |
      |               |            |                                                                 |
      |               |            |  不比对时，文件夹中包含compilation.bmodel一个文件即可           |
      |               |            |                                                                 |
      |               |            |  开启比对时，文件夹中要包含compilation.bmodel,                  |
      |               |            |  input_ref_data.dat, output_ref_data.dat三个文件                |
      +---------------+------------+-----------------------------------------------------------------+
      |   bmodel      |   string   |  与context_dir二选一, 直接指定.bmodel文件,默认不开启比对        |
      +---------------+------------+-----------------------------------------------------------------+
      |    devid      |   int      |  可选, 在多芯平台上通过id指定运行设备, 默认是0                  |
      +---------------+------------+-----------------------------------------------------------------+
      |   compare     |   bool     |  可选, 0表示不开比对， 1表示开启比对                            |
      +---------------+------------+-----------------------------------------------------------------+
      |   accuracy_f  |   float    |  可选, 指定浮点数据比对误差门限，默认是0.01                     |
      +---------------+------------+-----------------------------------------------------------------+
      |   accuracy_i  |   int      |  可选, 指定整数数据比对误差门限，默认是0                        |
      +---------------+------------+-----------------------------------------------------------------+
      |    shapes     |  string    |  可选, 指定测试时的输入shapes, 默认用bmodel的编译输入shape.     |
      |               |            |                                                                 |
      |               |            |  格式为‘[x,x,x,x],[x,x]’,                                       |
      |               |            |  对应于模型的输入顺序和个数                                     |
      +---------------+------------+-----------------------------------------------------------------+
      |   loopnum     |   int      |  可选, 指定连续运行次数，默认是1                                |
      +---------------+------------+-----------------------------------------------------------------+
      |  thread_num   |   int      |  可选, 指定运行线程数目，默认是1，测试多线程正确性              |
      +---------------+------------+-----------------------------------------------------------------+
      |   net_idx     |   int      |  可选, 在包含多个网络的bmodel通过序号选择运行用的net            |
      +---------------+------------+-----------------------------------------------------------------+
      |  stage_idx    |   int      |  可选, 在包含多个stage的bmodel通过序号选择运行用的stage         |
      +---------------+------------+-----------------------------------------------------------------+
      |  subnet_time  |   bool     |  可选, 是否显示bmodel的subnet时间                               |
      +---------------+------------+-----------------------------------------------------------------+

bmrt_test输出
____________________________________________

  .. code-block:: shell

      [BMRT][bmrt_test:981] INFO:net[vgg16_8_bmnetc] stage[0], launch total time is 19590 us (npu 19489 us, cpu 101 us)  (1)
      [BMRT][bmrt_test:984] INFO:+++ The network[vgg16_8_bmnetc] stage[0] output_data +++
      [BMRT][print_array:647] INFO:output data #0 shape: [4 32 120 68 ] < 3.49693 4.07723 4.30039 4.14311 4.11042 4.23445 4.23644 4.23897 4.23897 4.23897 4.23897 4.23897 4.23897 4.23897 4.23897 4.23897 ... > len=1044480  (2)
      [BMRT][print_array:647] INFO:output data #1 shape: [4 32 60 34 ] < 3.523 3.94491 4.09504 4.02145 3.95682 3.96846 3.96972 3.97314 3.9728 3.9728 3.9728 3.9728 3.9728 3.9728 3.9728 3.9728 ... > len=261120
      [BMRT][print_array:647] INFO:output data #2 shape: [4 32 30 17 ] < 4.18294 5.16457 5.26347 5.16108 5.0436 4.99669 4.99279 4.99279 4.99279 4.99279 4.99279 4.99651 5.02305 5.0925 5.23303 5.24913 ... > len=65280
      [BMRT][bmrt_test:1029] INFO:load input time(s): 0.008511 (3)
      [BMRT][bmrt_test:1030] INFO:calculate  time(s): 0.019594
      [BMRT][bmrt_test:1031] INFO:get output time(s): 0.006001 (4)
      [BMRT][bmrt_test:1032] INFO:compare    time(s): 0.002886

  主要关注点：
    (1) 模型的纯推理时间，不包含加载输入和获取输出时间
    (2) 推理数据展示，如开启比对，会显示比对成功等式信息
    (3) 用s2d加载输入数据时间, 通常前处理会将数据直接放到设备上，没有这个时间消耗
    (4) 用d2s取出输出数据时间, 通常表示在pcie上的数据传输时间，在soc上可以用mmap, 会更快

bmrt_test常用方法
____________________________________________

  .. code-block:: shell

      bmrt_test --context_dir bmodel_dir  # 运行bmodel，并比对数据，bmodel_dir中要包含compilation.bmodel/input_ref_data.dat/output_ref_data.dat
      bmrt_test --context_dir bmodel_dir  --compare=0 # 运行bmodel，bmodel_dir中要包含compilation.bmodel
      bmrt_test --bmodel xxx.bmodel # 直接运行bmodel，不比对数据
      bmrt_test --bmodel xxx.bmodel --stage_idx 0  --shapes "[1,3,224,224]" # 运行多stage的bmodel模型，指定运行stage0的bmodel

      # 以下命令是通过环境变量使用bmruntime提供的功能，其他应用程序也可以使用
      BMRUNTIME_ENABLE_PROFILE=1 bmrt_test --bmodel xxx.bmodel # 生成profile数据：bmprofile_data-x
      BMRT_SAVE_IO_TENSORS=1 bmrt_test --bmodel xxx.bmodel  # 将模型推理的数据保存成input_ref_data.dat.bmrt和output_ref_data.dat.bmrt


比对数据生成与验证举例
____________________________________________

1. 模型编译完成后，进行比对运行

    编译模型时要带上\--cmp=True，默认是开启的，就会在编译输出文件夹中生成input_ref_data.dat和output_ref_data.dat文件

    接着执行‘bmrt_test \--context_dir bmodel_dir’，便可验证模型推理数据正确性

2. pytorch原始模型和编译出的bmodel数据比对

    将pytorch模型的输入input_data和输出output_data转为numpy的array(torch的tensor可以用tensor.numpy())，然后存文件(见以下代码)

    .. code-block:: python

        # 单输入和单输出情况
        input_data.astype(np.float32).tofile("input_ref_data.dat")  # astype要根据bmodel的输入数据类型转换
        output_data.astype(np.float32).tofile("output_ref_data.dat")  # astype要根据bmodel的输出数据类型转换

        # 多输入和多输出情况
        with open("input_ref_data.dat", "wb") as f:
            for input_data in input_data_list:
                f.write(input_data.astype(np.float32).tobytes())  # astype要根据bmodel的输入数据类型转换
        with open("output_ref_data.dat", "wb") as f:
            for output_data in output_data_list:
                f.write(output_data.astype(np.float32).tobytes())  # astype要根据bmodel的输出数据类型转换

    把生成的input_ref_data.dat和output_ref_data.dat放到bmodel_dir文件夹下
    然后‘bmrt_test \--context_dir bmodel_dir’, 看看结果是否出比对错误

常见问题
_________________

1. 编译模型时出现数据比对错误？

我们bmcompiler内部用的是0.01作为比对阈值，在少数情况下可能会超出范围而报错。

如果某一层实现的有问题，会出现成片比对错误，这时需要向我们开发人员反馈。

如果随机位置的零星错误，可能是个别值计算误差引起的。原因是编译时用的是随机数据，不排除会出现这种情况，所以建议编译时先加上 \--cmp 0，在实际业务程序上验证结果是否正确

还有一种可能是网络中存在随机算子(如uniform_random)或者排序算子(如topk、nms、argmin等),由于在前面计算过程会产生输入数据的浮点尾数误差，即使很小也会导致排序结果的index不同。 这种情况下，可以看到比对出错的数据顺序上有差异，只能到实际业务上去测试