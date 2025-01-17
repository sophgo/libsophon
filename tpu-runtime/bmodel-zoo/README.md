# 简介
存放tpu-runtime回归使用的bmodel

## 注意事项
    tpu-runtime的回归测试搜寻regression_models下包含compilation.bmodel的目录，并通过目录命名来区分任务类别，在添加新模型时请一定规范命名，请勿轻易更换已存在的模型，尽量精简模型保障回归速度。

## 目录结构
    bmodel-zoo
    |──lfs_cmd.sh
    |──README.md
    └──regression_models/
        |──dynamic/         #存放动态编译模型
        |──multi_subnet/    #存放CPU子网/多子网模型
        └──static/          #存放静态单子网模型

## 模型存放规范
### 模型目录命名
单核编译模型:以`模型名_芯片架构_精度`命名，例如`Abs_bm1688_f32`
多核编译模型:以`模型名_芯片架构_精度_核数量`命名，例如`Abs_bm1688_f32_2core`

### 模型目录必要文件
    模型文件夹
    |──compilation.bmodel
    |──input_ref_data.dat
    |──output_ref_data.dat
    |──tensor_location.json
    └──final.mlir

## 模型获取方法
首次运行需要先安装git-lfs
执行`source lfs_cmd.sh`后，在bmodel-zoo根目录下使用`lfs_pull path/to/models`

## 模型上传
已track的格式包括:` .bmodel .dat .json .mlir `
                以上类型文件直接`git add `正常提交即可
其余格式若文件较大，请先执行`git lfs install`, 使用 git lfs tarck 后再提交，例如`git lfs tarck *.bmodel`