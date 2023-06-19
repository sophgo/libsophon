BModel
=============


BModel 介绍
_____________

bmodel是面向算能TPU处理器的深度神经网络模型文件格式。
通过模型编译器工具(如bmnetc/bmnett等)生成，包含一个至多个网络的参数信息，如输入输出等信息。
并在runtime阶段作为模型文件被加载和使用。

bmodel也作为BMLang编程语言的编译输出文件，由BMLang编译阶段生成，包含一个或多个BMLang功能Function的
参数、输入输出等信息。

多stage bmodel说明：

  bmodel里stage是用多种input_shape分别编译出bmodel，然后用tpu_model将多个bmodel合并到一起成为一个bmodel，而里面包含的每个bmodel是一个stage。stage_num就是指合并的bmodel个数。未经过合并的bmodel的stage_num=1。当以某种shape运行模型时，bmruntime会自动选择有相同输入shape的bmodel运行。

  通过选择几种常用的输入shape合并，可以提升运行效率，并实现动态运行的效果
  比如分别以[1,3,200,200]，[2,3,200,200]的输入编译出两个bmodel，合并后运行如果以[2,3,200,200]输入运行，则会自动找[2,3,200,200]的那个bmodel运行

  也可以分别以[1,3,200,200]，[1,3,100,100]的输入编译出两个bmodel，达到支持200x200和100x100输入的模型

静态bmodel说明：
  
1. 静态bmodel保存的是芯片上可直接使用的固定参数原子操作指令，TPU可以自动读取该原子操作指令，流水执行，中间无中断。

2. 静态bmodel被执行时，模型输入大小必须和编译时的大小相同。

3. 由于静态接口简单稳定，在新的sdk下编译出来的模型通常能在旧机器上运行，不用更新firmware刷机。需要注意的是，有些模型虽然指定的是静态编译，但有些算子必需有TPU内部mcu参与或host cpu参与，如排序、nms、where、detect_out这类逻辑运算比较多的算子，该部分会被切分成子网，用动态方式实现。如果更新sdk重新编译的这类部分是动态的模型，最好刷机或更新firmware，以保证sdk和runtime是一致的。（可以通过tpu_model \--info xx.bmodel的输出来判断，如果是static且subnet number为1时，是纯静态网络，具体可见tpu_model使用章节）。

4. 如果输入的shape只有固定离散的几种情况，可以使用上面说的多stage bmodel来达到动态模型的效果。

动态bmodel说明：

1. 动态bmodel保存的是每个算子的参数信息，并不能直接在TPU上运行。需要TPU内部的mcu逐层解析参数，进行shape及dtype推理，调用原子操作来实现具体的功能，故运行效率比静态bmodel稍差。

2. 在bm168x平台上运行时，最好打开icache，否则运行比较慢。

3. 编译时，通过shapes参数来指定支持的最大shape。在实际运行时，除了c维，其他维均支持可变。通常在可变shape情况过多，考虑用动态编译，否则推荐用多stage bmodel模式。

4. 动态bmodel为了保证参数可扩展以及兼容，会保证新sdk的runtime能运行旧版本sdk编译出的动态bmodel。通常建议换新sdk后刷机，保证两者版本一致。

tpu_model 使用
_____________

通过tpu_model工具，可以查看bmodel文件的参数信息，可以将多个网络bmodel分解成多个单网络的bmodel，也可以将多个网络的bmodel合并成一个bmodel。

目前支持以下六种使用方法：

1. 查看简要信息(比较常用)

  ``tpu_model --info xxx.bmodel``


  输出信息如下

  .. code-block:: shell

    bmodel version: B.2.2                         # bmodel的格式版本号
    chip: BM1684                                  # 支持的芯片类型
    create time: Mon Apr 11 13:37:45 2022         # 创建时间

    ==========================================    # 网络分割线，如果有多个net，会有多条分割线
    net 0: [informer_frozen_graph]  static        # 网络名称为informer_frozen_graph， 为static类型网络（即静态网络），如果是dynamic，为动态编译网络
    ------------                                  # stage分割线，如果每个网络有多个stage，会有多个分割线
    stage 0:                                      # 第一个stage信息
    subnet number: 41                             # 该stage中子网个数，这个是编译时切分的，以支持在不同设备切换运行。通常子网个数
                                                  # 越少越好
    input: x_1, [1, 600, 9], float32, scale: 1    # 输入输出信息：名称、形状、量化的scale值
    input: x_3, [1, 600, 9], float32, scale: 1
    input: x, [1, 500, 9], float32, scale: 1
    input: x_2, [1, 500, 9], float32, scale: 1
    output: Identity, [1, 400, 7], float32, scale: 1

    device mem size: 942757216 (coeff: 141710112, instruct: 12291552, runtime: 788755552)  # 该模型在TPU上内存占用情况（以byte为单位)，格式为： 总占用内存大小（常量内存大小，指令内存大小, 运行时数据内存占用大小)
    host mem size: 8492192 (coeff: 32, runtime: 8492160)   # 宿主机上内存占用情况（以byte为单位），格式为： 总占用内存大小（常量内存大小，运行时数据内存大小）



2. 查看详细参数信息

  ``tpu_model --print xxx.bmodel``


3. 分解

  ``tpu_model --extract xxx.bmodel``

  将一个包含多个网络多种stage的bmodel分解成只包含一个网络的一个stage的各个bmodel，分解出来的bmodel按照net序号和stage序号，命名为bm_net0_stage0.bmodel、bm_net1_stage0.bmodel等等。


4. 合并

  ``tpu_model --combine a.bmodel b.bmodel c.bmodel -o abc.bmodel``

  将多个bmodel合并成一个bmodel，-o用于指定输出文件名，如果没有指定，则默认命名为compilation.bmodel。

  多个bmodel合并后：

  * 不同net_name的bmodel合并，接口会根据net_name选择对应的网络进行推理

  * 相同net_name的bmodel合并，会使该net_name的网络可以支持多种stage(也就是支持不同的input shape)。接口会根据用户输入的shape，在该网络的多个stage中选择。对于静态网络，它会选择shape完全匹配的stage；对于动态网络，它会选择最靠近的stage。

  限制：同一个网络net_name，使用combine时，要求都是静态编译，或者都是动态编译。暂时不支持相同net_name的静态编译和动态编译的combine。

5. 合并文件夹

  ``tpu_model --combine_dir a_dir b_dir c_dir -o abc_dir``

  同combine功能类似，不同的是，该功能除了合并bmodel外，还会合并用于测试的输入输出文件。它以文件夹为单位合并，文件夹中必须包含经过编译器生成的三个文件：input_ref_data.dat, output_ref_data.dat, compilation.bmodel。

6. 导出二进制数据

  ``tpu_model --dump xxx.bmodel start_offset byte_size out_file``

  将bmodel中的二进制数据保存到一个文件中。通过print功能可以查看所有二进制数据的[start, size]，对应此处的start_offset和byte_size。
