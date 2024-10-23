
#include <stdio.h>
#include <string>
#include <vector>
#include "bmodel.hpp"


using namespace std;
using namespace bmodel;

// combine bmodels
typedef struct {
  uint32_t net_idx;
  uint32_t stage_idx;
  char *input;
  size_t input_size;
  char *output;
  size_t output_size;
} NET_INDEX_T;

typedef struct {
  shared_ptr<ModelCtx> model_ctx;
  ifstream input_f;
  ifstream output_f;
  vector<shared_ptr<NET_INDEX_T>> net_index_v;
} MODEL_CTX_T;

void print(const string &filename);
void show(const NetParameter *parameter, bool dynamic = false);
void show(const string &filename);
void show_weight(const string &filename);
void update_weight(int argc, char **argv);
void encrypt(int argc, char **argv);
void decrypt(int argc, char **argv);
void extract(const string &filename);
void combine_bmodels(ModelGen &model_gen, vector<shared_ptr<MODEL_CTX_T>>& model_vec, bool is_dir = false);
void combine_bmodels(int argc, char **argv, bool is_dir = false);
size_t combine_bmodels_coeff(ModelGen &model_gen, vector<shared_ptr<MODEL_CTX_T>>& model_vec, bool is_dir = false);
void combine_bmodels_coeff(int argc, char **argv, bool is_dir = false);
void dump_binary(int argc, char **argv);
void dump_kernel_module(int argc, char **argv);
void update_kernel_module(int argc, char **argv);
void update_cpuop_module(int argc, char **argv);