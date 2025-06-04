#include <gtest/gtest.h>
#include <algorithm>
#include "cpu_random_uniform.h"

class CPURandomUnifromTest : public ::testing::Test {
protected:
    bmcpu::cpu_random_uniformlayer layer;
    cpu_random_uniform_param_t param;
    std::vector<int> input;
    std::vector<float> output;
    std::vector<std::vector<int>> output_shapes;
    void SetUp() override {
        const size_t len = 256;
        input.push_back(len);
        output.resize(len);
        std::vector<float *> input_tensors(1, reinterpret_cast<float *>(input.data()));
        std::vector<std::vector<int>> input_shapes{{1}};
        std::vector<float *> output_tensors(1, output.data());
        output_shapes = std::vector<std::vector<int>>{{len}};
        layer.set_common_param(input_tensors, input_shapes, output_tensors, output_shapes);

        param.lower = 0;
        param.upper = 1;
        param.seed = 1;
        param.dim = 1;
        param.seed = 0;
    }
};

TEST_F(CPURandomUnifromTest, process)
{
    layer.process(&param, sizeof(param));
}

TEST_F(CPURandomUnifromTest, randomness)
{
    layer.process(&param, sizeof(param));
    ASSERT_FALSE(std::all_of(output.begin(), output.end(), [](float v) { return v == 0; }));
    auto first = output;
    layer.process(&param, sizeof(param));
    auto second = output;
    ASSERT_FALSE(std::equal(first.begin(), first.end(), second.begin()));
}
