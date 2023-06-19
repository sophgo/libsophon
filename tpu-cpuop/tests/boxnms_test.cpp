#include<stdio.h>
#include "cpu_box_nms.h"

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
    float input_data[] ={
        0, 0.5, 0.1, 0.1, 0.2, 0.2,
        1, 0.4, 0.1, 0.1, 0.2, 0.2, 
        0, 0.3, 0.1, 0.1, 0.14, 0.14,
        2, 0.6, 0.5, 0.5, 0.7, 0.8,

        3, 0.5, 0.1, 0.1, 0.2, 0.2,
        1, 0.4, 0.1, 0.1, 0.2, 0.2, 
        0, 0.3, 0.1, 0.1, 0.14, 0.14,
        2, 0.25, 0.5, 0.5, 0.7, 0.8
    };
    vector<int>input_shape{2,4,6};
    vector<decltype(input_shape)> input_shapes = {input_shape};
    vector<float*> input_tensors={input_data};

    cpu_box_nms_param_t param;
    param.background_id = -1;
    param.overlap_thresh= 0.1;
    param.valid_thresh = 0.35;
    param.in_format = 0;
    param.coord_start = 2;
    param.id_index = 0;
    param.score_index = 1;
    param.force_suppress = 1;
    param.topk = 2;
    param.out_format = 1;
    float output_ptr[sizeof(input_data)];
    vector<int> output_shape;
    vector<decltype(output_shape)> output_shapes = {output_shape};
    vector<float*> output_tensors = {output_ptr};

    bmcpu::cpu_boxnmslayer layer;
    layer.set_common_param(
        input_tensors, input_shapes, output_tensors, output_shapes
    );
    layer.process(&param, sizeof(param));
    print_vector(output_shapes[0]);
    for(int i=0; i<2*4; i++){
        vector<float> row(output_ptr+i*6, output_ptr+i*6+6);
        print_vector(row);
    }

    return 0;
}
