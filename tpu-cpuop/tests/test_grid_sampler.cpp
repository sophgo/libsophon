#include <gtest/gtest.h>
#include <algorithm>
#include "cpu_grid_sampler.h"

class CPUGridSamplerTest : public ::testing::Test {
protected:
    bmcpu::cpu_grid_samplerlayer layer;
    cpu_grid_sampler_param_t param;
    std::vector<float> input;
    std::vector<float> grid;
    std::vector<float> output;
    std::vector<std::vector<int>> input_shapes;
    std::vector<std::vector<int>> output_shapes;
    void SetUp() override {
        output_shapes = input_shapes = std::vector<std::vector<int>>{{1, 1, 3, 3}};
        input_shapes.push_back({1, 3, 3, 2});
        const size_t input_len = 9;
        input.resize(input_len);
        output.resize(input_len);
        for (int i = 0; i < input_len; ++i)
            input[i] = i;
        grid = std::vector<float>{
            -2., -1.,
            -1., -1.,
             0., -1.,
            -1.,  0.,
             0.,  0.,
             1.,  0.,
             0.,  1.,
             1.,  1.,
             2.,  1.,
        };
        std::vector<float *> input_tensors;
        input_tensors.push_back(input.data());
        input_tensors.push_back(grid.data());
        std::vector<float *> output_tensors(1, output.data());
        layer.set_common_param(input_tensors, input_shapes, output_tensors, output_shapes);
    }
};

TEST_F(CPUGridSamplerTest, bilinearZerosTrue)
{
    param.mode = GridSamplerBilinear;
    param.padding_mode = GridSamplerZeros;
    param.align_corners = true;
    std::vector<float> expect{0., 0., 1., 3., 4., 5., 7., 8., 0.};
    layer.process(&param, sizeof(param));
    ASSERT_TRUE(std::equal(output.begin(), output.end(), expect.begin()));
}

TEST_F(CPUGridSamplerTest, bilinearZerosFalse)
{
    param.mode = GridSamplerBilinear;
    param.padding_mode = GridSamplerZeros;
    param.align_corners = false;
    std::vector<float> expect{0., 0., 0.5, 1.5, 4., 2.5, 3.5, 2., 0.};
    layer.process(&param, sizeof(param));
    ASSERT_TRUE(std::equal(output.begin(), output.end(), expect.begin()));
}

TEST_F(CPUGridSamplerTest, bilinearBorderTrue)
{
    param.mode = GridSamplerBilinear;
    param.padding_mode = GridSamplerBorder;
    param.align_corners = true;
    std::vector<float> expect{0., 0., 1., 3., 4., 5., 7., 8., 8.};
    layer.process(&param, sizeof(param));
    ASSERT_TRUE(std::equal(output.begin(), output.end(), expect.begin()));
}

TEST_F(CPUGridSamplerTest, bilinearReflectionTrue)
{
    param.mode = GridSamplerBilinear;
    param.padding_mode = GridSamplerReflection;
    param.align_corners = true;
    std::vector<float> expect{1., 0., 1., 3., 4., 5., 7., 8., 7.};
    layer.process(&param, sizeof(param));
    ASSERT_TRUE(std::equal(output.begin(), output.end(), expect.begin()));
}
