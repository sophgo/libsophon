#include "tpu_model.h"
#include "model_interface.h"

class BModel {
public:
  BModel() : model_gen_(NULL) {};
  virtual ~BModel() {
    if (model_gen_ != NULL) {
      delete model_gen_;
    }
  }

  void load_bmodel_data(const void* bmodel_data, uint64_t size) {
    shared_ptr<MODEL_CTX_T> model_info(new MODEL_CTX_T);
    model_info->model_ctx = make_shared<ModelCtx>(bmodel_data, size);
    if (model_info->model_ctx == NULL || !(*model_info->model_ctx)) {
      printf("bmodel data is not correct");
      exit(1);
    }
    model_vec_.push_back(model_info);
  }
  uint64_t combine_bmodel() {
    assert(model_gen_ == NULL);
    model_gen_ = new bmodel::ModelGen(0);
    return combine_bmodels_coeff((*model_gen_), model_vec_, false);
    // return model_gen_->BufferSize();
  }
  void save_bmodel_data(void* bmodel_data) {
    return model_gen_->Save(bmodel_data);
  }

private:
  vector<shared_ptr<MODEL_CTX_T>> model_vec_;
  ModelGen *model_gen_;
};

tpum_handel_t tpu_model_create() {
  BModel* p_bmodel = new BModel();
  return (tpum_handel_t)p_bmodel;
}

void tpu_model_add(tpum_handel_t p_bmodel, const void* bmodel_data, unsigned long long size) {
  return ((BModel*)p_bmodel)->load_bmodel_data(bmodel_data, size);
}
unsigned long long tpu_model_combine(tpum_handel_t p_bmodel) {
  return ((BModel*)p_bmodel)->combine_bmodel();
}
void tpu_model_save(tpum_handel_t p_bmodel, void* bmodel_data) {
  return ((BModel*)p_bmodel)->save_bmodel_data(bmodel_data);
}
void tpu_model_destroy(tpum_handel_t p_bmodel) {
  if (p_bmodel != NULL) {
    delete (BModel*)p_bmodel;
  }
}
