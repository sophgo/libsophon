#include "model_interface.h"
#include <iostream>
#include <fstream>
#include <string>
#include <memory>

// mode0, model1
int main(int argc, char **argv) {
  auto model0 = argv[1];
  auto model1 = argv[2];
  std::fstream file0, file1;
  file0.open(model0, std::ios::binary | std::ios::in);
  if (!file0) {
    std::cout << "File[" << model0 << "] open failed." << std::endl;
    exit(-1);
  }
  file1.open(model1, std::ios::binary | std::ios::in);
  if (!file1) {
    std::cout << "File[" << model1 << "] open failed." << std::endl;
    exit(-1);
  }
  file0.seekg(0, std::ios::end);
  size_t length0 = file0.tellg();
  std::unique_ptr<char> bmodel0(new char[length0]);
  std::cout << "File[" << model0 << "] size: " << length0 << std::endl;
  file0.seekg(0, std::ios::beg);
  file0.read(bmodel0.get(), length0);
  file1.seekg(0, std::ios::end);
  size_t length1 = file1.tellg();
  std::unique_ptr<char> bmodel1(new char[length1]);
  std::cout << "File[" << model1 << "] size: " << length1 << std::endl;
  file1.seekg(0, std::ios::beg);
  file1.read(bmodel1.get(), length1);
  file0.close();
  file1.close();

  auto tpu_handle = tpu_model_create();
  tpu_model_add(tpu_handle, bmodel0.get(), length0);
  tpu_model_add(tpu_handle, bmodel1.get(), length1);
  auto length = tpu_model_combine(tpu_handle);
  std::unique_ptr<char> bmodel_out(new char[length]);
  tpu_model_save(tpu_handle, bmodel_out.get());
  tpu_model_destroy(tpu_handle);
  std::fstream file_out;
  file_out.open("bm_combine.bmodel", std::ios::binary | std::ios::out);
  file_out.write(bmodel_out.get(), length);
  file_out.close();
}
