#include<stdio.h>
#include "cpu_roialignlayer.h"

using namespace std;
template<typename T>
void print_vector(const vector<T>& data){
    cout<<"{ ";
    for(auto d: data){
        cout<<d<<" ";
    }
    cout<<"}"<<endl;
}


int main(){
    const int x_batch = 2;
    const int x_channel = 3;
    const int x_height=20;
    const int x_width = 20;
    const int x_size =x_batch*x_channel*x_height*x_width;


    float input_data[x_size];
    for(int i=0; i<x_size; i++){
        input_data[i] = (float)i;
    }
    float roi_data[]={
        0, 0, 0, x_height, x_width/2,
        1, 0, 0, x_height/2, x_width
    };
    vector<int> input_shape{x_batch, x_channel, x_height, x_width};
    vector<int> roi_shape{x_batch,5};

    vector<decltype(input_shape)> input_shapes = {input_shape, roi_shape};
    vector<float*> input_tensors={input_data, roi_data};

    cpu_roi_align_param_t param;
    const int pool_h = 4;
    const int pool_w = 3;
    param.pooled_height = pool_h;
    param.pooled_width = pool_w;
    param.position_sensitive = 0;
    param.sampling_ratio = 2;
    param.spatial_scale = 1.0;

    float output_ptr[x_batch*x_channel*pool_h*pool_w];
    vector<int> output_shape;
    vector<decltype(output_shape)> output_shapes = {output_shape};
    vector<float*> output_tensors = {output_ptr};

    bmcpu::cpu_roialignlayer layer;
    layer.set_common_param(
        input_tensors, input_shapes, output_tensors, output_shapes
    );
    layer.process(&param, sizeof(param));
    print_vector(output_shapes[0]);
    for(int i=0; i<pool_h*x_batch*x_channel; i++){
        vector<float> row(output_ptr+i*pool_w, output_ptr+(i+1)*pool_w);
        print_vector(row);
    }

    return 0;
}
