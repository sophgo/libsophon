#include "cpu_rpnproposallayer.h"

#define rpn_max(a,b) (((a) > (b)) ? (a) : (b))
#define rpn_min(a,b) (((a) > (b)) ? (b) : (a))

namespace bmcpu {

#define MAX_ROI_NUM 200

int cpu_rpnproposallayer::process(void *param, int param_size)
{
  setParam(param, param_size);

  int* anchors = NULL;
  int  anchors_nums = 0;
  generate_anchors(anchors, anchors_nums);

  int map_channel = input_shapes_[1][1];
  int map_height = input_shapes_[1][2];
  int map_width = input_shapes_[1][3];
  CPU_ASSERT(map_channel == 4 * anchors_nums);
  CPU_ASSERT(input_shapes_[0][1] == 2 * anchors_nums);

  const float* src_info = input_tensors_[2];
  int src_info_cnt = input_shapes_[2][0]*input_shapes_[2][1];
  float* src_infoy_tmp = new float[src_info_cnt];
  memcpy(src_infoy_tmp, input_tensors_[2], src_info_cnt*sizeof(float)) ;
  int src_height = src_info[0];
  int src_width = src_info[1];
  float src_scale = src_info[2];

  int step = map_width * map_height;
  float* shift_x = new float[step];
  float* shift_y = new float[step];
  for(int i = 0; i < map_height; ++i) {
    for(int j = 0; j < map_width; j++) {
      shift_x[i * map_width + j] = j * feat_stride_;
      shift_y[i * map_width + j] = i * feat_stride_;
    }
  }

  vector<abox> aboxes;
  //resreve the capacity of aboxes, to elimate performance issue caused by aboxes memory increasing.
  aboxes.reserve(201);
  const float* m_score = input_tensors_[0];
  float *m_box = input_tensors_[1];
  int m_score_cnt = input_shapes_[0][0]*input_shapes_[0][1]*input_shapes_[0][2]*input_shapes_[0][3];
  float* m_score_tmp = new float[m_score_cnt];
  memcpy(m_score_tmp, input_tensors_[0], m_score_cnt*sizeof(float)) ;
  int m_box_cnt = input_shapes_[1][0]*input_shapes_[1][1]*input_shapes_[1][2]*input_shapes_[1][3];
  float* m_box_tmp = new float[m_box_cnt];
  memcpy(m_box_tmp, input_tensors_[1], m_box_cnt*sizeof(float)) ;
  float* local_anchors = new float[4 * map_height * map_width];
  for(int idx = 0; idx < anchors_nums; idx++) {
    //proposal_local_anchor
    rpn_set(step, (float)(anchors[idx * 4 + 0]), local_anchors + 0 * step);
    rpn_set(step, (float)(anchors[idx * 4 + 1]), local_anchors + 1 * step);
    rpn_set(step, (float)(anchors[idx * 4 + 2]), local_anchors + 2 * step);
    rpn_set(step, (float)(anchors[idx * 4 + 3]), local_anchors + 3 * step);
    rpn_axpy(step, (float)(1), shift_x, local_anchors + 0 * step);
    rpn_axpy(step, (float)(1), shift_x, local_anchors + 2 * step);
    rpn_axpy(step, (float)(1), shift_y, local_anchors + 1 * step);
    rpn_axpy(step, (float)(1), shift_y, local_anchors + 3 * step);

    //bbox_tranform_inv
    rpn_axpy(2 * step, (float)(-1), local_anchors + 0 * step, local_anchors + 2 * step);
    rpn_add_scalar(2 * step, (float)(1), local_anchors + 2 * step);
    rpn_axpy(2 * step, (float)(0.5), local_anchors + 2 * step, local_anchors + 0 * step);

    rpn_mul(2 * step, local_anchors + 2 * step, m_box + (idx * 4 + 0) * step, m_box + (idx * 4 + 0) * step);
    rpn_add(2 * step, local_anchors + 0 * step, m_box + (idx * 4 + 0) * step, m_box + (idx * 4 + 0) * step);

    rpn_exp(2 * step, m_box + (idx * 4 + 2) * step, m_box + (idx * 4 + 2) * step);
    rpn_mul(2 * step, local_anchors + 2 * step, m_box + (idx * 4 + 2) * step, m_box + (idx * 4 + 2) * step);

    //NMS filter_boxs
    float localMinSize = min_size_ * src_scale;
    int four_step = 4 * map_height * map_width;
    int one_step = map_height * map_width;
    int offset_w, offset_h, offset_x, offset_y, offset_s;
    for (int h = 0; h < map_height; ++h) {
      for (int w = 0; w < map_width; ++w) {
        offset_x = h * map_width + w + four_step * idx;
        offset_y = offset_x + one_step;
        offset_w = offset_y + one_step;
        offset_h = offset_w + one_step;
        offset_s = one_step * anchors_nums + one_step * idx + h * map_width + w;
        float width = m_box[offset_w], height = m_box[offset_h];
        if (width < localMinSize || height < localMinSize || m_score[offset_s] < score_thresh_) {
          //Discard boxes
        } else {
          abox box;
          box.batch_ind = 0;
          box.x1 = m_box[offset_x] - 0.5f * width;
          box.y1 = m_box[offset_y] - 0.5f * height;
          box.x2 = m_box[offset_x] + 0.5f * width;
          box.y2 = m_box[offset_y] + 0.5f * height;
          box.x1 = rpn_min(rpn_max(box.x1, 0), src_width);
          box.y1 = rpn_min(rpn_max(box.y1, 0), src_height);
          box.x2 = rpn_min(rpn_max(box.x2, 0), src_width);
          box.y2 = rpn_min(rpn_max(box.y2, 0), src_height);
          box.area = (box.x2 - box.x1 + 1) * (box.y2 - box.y1 + 1);
          box.score = m_score[offset_s];

          int tgt_pos = aboxes.size();
          if(!aboxes.empty()) {
            //insert sort: descending order by score
            tgt_pos = aboxes.size() - 1;
            while(tgt_pos >= 0 && box.score > aboxes[tgt_pos].score) {
              --tgt_pos;
            }
            ++tgt_pos;
          }
          aboxes.insert(aboxes.begin() + tgt_pos, box);
        }
      }
    }
  }

  delete[] shift_x;
  delete[] shift_y;
  delete[] local_anchors;
  if(anchors != NULL) delete[] anchors;

  if (pre_nms_topN_ > 0) {
    int pos = rpn_min(pre_nms_topN_, aboxes.size());
    aboxes.erase(aboxes.begin() + pos, aboxes.end());
  }

  //Non Maximum Suppression
  nms(aboxes, nms_thresh_);

  if (post_nms_topN_ > 0) {
    int pos = rpn_min(post_nms_topN_, aboxes.size());
    aboxes.erase(aboxes.begin() + pos, aboxes.end());
  }

  float* top0 = output_tensors_[0];
  for(int i = 0; i < rpn_min(aboxes.size(), MAX_ROI_NUM); ++i) {
    top0[0] = aboxes[i].batch_ind;
    top0[1] = aboxes[i].x1;
    top0[2] = aboxes[i].y1;
    top0[3] = aboxes[i].x2;
    top0[4] = aboxes[i].y2;
    top0 += 5;
  }

  //update actual roi num
  (*output_shapes_)[0][0] = rpn_min(aboxes.size(), MAX_ROI_NUM);

  if(output_tensors_.size() > 1) {
    float* top1 = output_tensors_[1];
    for (int i = 0; i < rpn_min(aboxes.size(), MAX_ROI_NUM);++i) {
      top1[0] = aboxes[i].score;
      top1 ++;
    }
    (*output_shapes_)[1][0] = rpn_min(aboxes.size(), MAX_ROI_NUM);
  }
  memcpy(input_tensors_[2], src_infoy_tmp, src_info_cnt*sizeof(float));
  memcpy(input_tensors_[0], m_score_tmp, m_score_cnt*sizeof(float));
  memcpy(input_tensors_[1], m_box_tmp, m_box_cnt*sizeof(float));
  delete[] src_infoy_tmp;
  delete[] m_score_tmp;
  delete[] m_box_tmp;

  return 0;
}

void cpu_rpnproposallayer::nms(vector<abox> &aboxes, float nms_thresh)
{
  for(int i = 0; i < aboxes.size(); ++i) {
    for(int j = i + 1; j < aboxes.size();) {
      float xx1 = rpn_max(aboxes[i].x1, aboxes[j].x1);
      float yy1 = rpn_max(aboxes[i].y1, aboxes[j].y1);
      float xx2 = rpn_min(aboxes[i].x2, aboxes[j].x2);
      float yy2 = rpn_min(aboxes[i].y2, aboxes[j].y2);
      float w = rpn_max(float(0), xx2 - xx1 + 1);
      float h = rpn_max(float(0), yy2 - yy1 + 1);
      float inter = w * h;
      float ovr = inter / (aboxes[i].area + aboxes[j].area - inter);
      //Drop overlapped boxes
      if (ovr >= nms_thresh)
        aboxes.erase(aboxes.begin() + j);
      else
        ++j;
    }
  }
}

void cpu_rpnproposallayer::setParam(void *param, int param_size)
{
  layer_param_ = param;
  BMCPU_DECLARE_AND_UNPACK_PARAM(cpu_rpnproposal_param_t, rpnproposal_param, layer_param_, param_size);

  feat_stride_ = rpnproposal_param->feat_stride_;
  min_size_ = rpnproposal_param->min_size_;
  pre_nms_topN_ = rpnproposal_param->pre_nms_topN_;
  post_nms_topN_ = rpnproposal_param->post_nms_topN_;
  nms_thresh_ = rpnproposal_param->nms_thresh_;
  score_thresh_ = rpnproposal_param->score_thresh_;
  base_size_ = rpnproposal_param->base_size_;
  scales_num_ = rpnproposal_param->scales_num_;
  ratios_num_ = rpnproposal_param->ratios_num_;
  memcpy(anchor_scales_, rpnproposal_param->anchor_scales_, 5 * sizeof(int));
  memcpy(ratios_, rpnproposal_param->ratios_, 5 * sizeof(float));
}

vector<vector<float> > cpu_rpnproposallayer::ratio_enum(vector<float> anchor)
{
  vector<vector<float> > result;
  vector<float> reform_anchor;
  reform_anchor.push_back(anchor[2] - anchor[0] + 1); //w
  reform_anchor.push_back(anchor[3] - anchor[1] + 1); //h
  reform_anchor.push_back((anchor[2] + anchor[0]) / 2); //ctrx
  reform_anchor.push_back((anchor[3] + anchor[1]) / 2); //ctry

  float x_ctr = reform_anchor[2];
  float y_ctr = reform_anchor[3];
  float size = reform_anchor[0] * reform_anchor[1];
  for (int i = 0; i < ratios_num_; ++i)
  {
    float size_ratios = size / ratios_[i];
    float ws = round(sqrt(size_ratios));
    float hs = round(ws * ratios_[i]);
    vector<float> tmp;
    tmp.push_back(x_ctr - 0.5f * (ws - 1));
    tmp.push_back(y_ctr - 0.5f * (hs - 1));
    tmp.push_back(x_ctr + 0.5f * (ws - 1));
    tmp.push_back(y_ctr + 0.5f * (hs - 1));

    result.push_back(tmp);
  }
  return result;
}

vector<vector<float> > cpu_rpnproposallayer::scale_enum(vector<float> anchor)
{
  vector<vector<float> > result;
  vector<float> reform_anchor;
  reform_anchor.push_back(anchor[2] - anchor[0] + 1); //w
  reform_anchor.push_back(anchor[3] - anchor[1] + 1); //h
  reform_anchor.push_back((anchor[2] + anchor[0]) / 2); //ctrx
  reform_anchor.push_back((anchor[3] + anchor[1]) / 2); //ctry

  float x_ctr = reform_anchor[2];
  float y_ctr = reform_anchor[3];
  float w = reform_anchor[0];
  float h = reform_anchor[1];
  for (int i = 0; i < scales_num_; ++i) {
    float ws = w * anchor_scales_[i];
    float hs = h *  anchor_scales_[i];
    vector<float> tmp;
    tmp.push_back(x_ctr - 0.5f * (ws - 1));
    tmp.push_back(y_ctr - 0.5f * (hs - 1));
    tmp.push_back(x_ctr + 0.5f * (ws - 1));
    tmp.push_back(y_ctr + 0.5f * (hs - 1));

    result.push_back(tmp);
  }
  return result;
}

void cpu_rpnproposallayer::generate_anchors(int* &anchors, int& anchors_nums)
{
  vector<float> base_anchor;
  base_anchor.push_back(0);
  base_anchor.push_back(0);
  base_anchor.push_back(base_size_ - 1);
  base_anchor.push_back(base_size_ - 1);

  //enum ratio anchors
  vector<vector<float> > gen_anchors;
  vector<vector<float> > ratio_anchors = ratio_enum(base_anchor);
  for (int i = 0; i < ratio_anchors.size(); ++i)
  {
    vector<vector<float> > tmp = scale_enum(ratio_anchors[i]);
    gen_anchors.insert(gen_anchors.end(), tmp.begin(), tmp.end());
  }

  anchors_nums = gen_anchors.size();
  anchors = new int[anchors_nums * 4];
  for (int i = 0; i < gen_anchors.size(); ++i) {
    for (int j = 0; j < gen_anchors[i].size(); ++j) {
      anchors[i * 4 + j] = gen_anchors[i][j];
    }
  }
}

REGISTER_CPULAYER_CLASS(CPU_RPN, cpu_rpnproposal)

}
