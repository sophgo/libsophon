#include "tpu_model.h"
#include <iostream>

// show usage of tool
static void usage(char *argv[])
{
  cout << "Usage:" << endl;
  cout << "  " << argv[0] << endl
       << "    --info model_file : show brief model info" << endl
       << "    --print model_file : show detailed model info" << endl
       << "    --weight model_file : show model weight info" << endl
       << "    --update_weight dst_model dst_net dst_offset src_model src_net src_offset" << endl
       << "    --encrypt -model model_file -net net_name -lib lib_path -o out_file" << endl
       << "    --decrypt -model model_file -lib lib_path -o out_file" << endl
       << "    --extract model_file : extract one multi-net bmodel to multi one-net bmodels" << endl
       << "    --combine file1 .. fileN -o new_file: combine bmodels to one bmodel by filepath" << endl
       << "    --combine_dir dir1 .. dirN -o new_dir: combine bmodels to one bmodel by directory path" << endl
       << "    --combine_coeff file1 .. fileN -o new_dir: combine bmodels to one bmodel by filepath, all models' coeff is same" << endl
       << "    --combine_coeff_dir dir1 .. dirN -o new_file: combine bmodels to one bmodel by directory path, all models' coeff is same" << endl
       << "    --dump model_file start_offset byte_size out_file: dump binary data to file from bmodel" << endl
       << "    --version show tool version" << endl
       << "    --kernel_dump model_file -o kernel_file_name : dump kernel_module file" << endl
       << "    --kernel_update model_file kernel_name : add/update kernel_module file" << endl
       << "    --custom_ap_update model_file libcpuop_file : add/update custom libcpuop file" << endl
       << endl;
}

int main(int argc, char **argv)
{
  if (argc < 2) {
    usage(argv);
    exit(-1);
  }

  string cmd = argv[1];
  if (cmd == "--print") {
    print(argv[2]);
  } else if (cmd == "--info") {
    show(argv[2]);
  } else if (cmd == "--weight") {
    show_weight(argv[2]);
  } else if (cmd == "--update_weight") {
    update_weight(argc, argv);
  } else if (cmd == "--encrypt") {
    encrypt(argc, argv);
  } else if (cmd == "--decrypt") {
    decrypt(argc, argv);
  } else if (cmd == "--extract") {
    extract(argv[2]);
  } else if (cmd == "--combine") {
    combine_bmodels(argc, argv);
  } else if (cmd == "--combine_dir") {
    combine_bmodels(argc, argv, true);
  } else if (cmd == "--combine_coeff") {
    combine_bmodels_coeff(argc, argv);
  } else if (cmd == "--combine_coeff_dir") {
    combine_bmodels_coeff(argc, argv, true);
  } else if (cmd == "--dump") {
    dump_binary(argc, argv);
  } else if (cmd == "--version") {
    printf("%s\n", VER);
  } else if (cmd == "--kernel_dump") {
    dump_kernel_module(argc, argv);
  } else if (cmd == "--kernel_update") {
    update_kernel_module(argc, argv);
  } else if (cmd == "--custom_ap_update") {
    update_cpuop_module(argc, argv);
  }else {
    usage(argv);
    exit(-1);
  }

  return 0;
}
