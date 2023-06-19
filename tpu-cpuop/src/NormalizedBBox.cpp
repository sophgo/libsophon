#include "NormalizedBBox.h"

#include <algorithm>
#include <memory.h>

namespace bmcpu {

// ===================================================================

#ifndef _MSC_VER
const int NormalizedBBox::kXminFieldNumber;
const int NormalizedBBox::kYminFieldNumber;
const int NormalizedBBox::kXmaxFieldNumber;
const int NormalizedBBox::kYmaxFieldNumber;
const int NormalizedBBox::kLabelFieldNumber;
const int NormalizedBBox::kDifficultFieldNumber;
const int NormalizedBBox::kScoreFieldNumber;
const int NormalizedBBox::kSizeFieldNumber;
#endif  // !_MSC_VER

NormalizedBBox::NormalizedBBox()  { 
  SharedCtor();
}


NormalizedBBox::NormalizedBBox(const NormalizedBBox& from) {
  SharedCtor();
  MergeFrom(from);
}

void NormalizedBBox::SharedCtor() {
  _cached_size_ = 0;
  xmin_ = 0;
  ymin_ = 0;
  xmax_ = 0;
  ymax_ = 0;
  label_ = 0;
  difficult_ = false;
  score_ = 0;
  size_ = 0;
  memset(_has_bits_, 0, sizeof(_has_bits_));
}

NormalizedBBox::~NormalizedBBox() {
  SharedDtor();
}

void NormalizedBBox::SharedDtor() {

}

void NormalizedBBox::SetCachedSize(int size) const {
  _cached_size_ = size;
}



NormalizedBBox* NormalizedBBox::New() const {
  return new NormalizedBBox;
}

void NormalizedBBox::Clear() {
#define OFFSET_OF_FIELD_(f) (reinterpret_cast<char*>(      \
  &reinterpret_cast<NormalizedBBox*>(16)->f) - \
   reinterpret_cast<char*>(16))

#define ZR_(first, last) do {                              \
    size_t f = OFFSET_OF_FIELD_(first);                    \
    size_t n = OFFSET_OF_FIELD_(last) - f + sizeof(last);  \
    memset(&first, 0, n);                                \
  } while (0)

  if (_has_bits_[0 / 32] & 255) {
    ZR_(xmin_, size_);
  }

#undef OFFSET_OF_FIELD_
#undef ZR_

  memset(_has_bits_, 0, sizeof(_has_bits_));
}


int NormalizedBBox::ByteSize() const {
  int total_size = 0;

  if (_has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    // optional float xmin = 1;
    if (has_xmin()) {
      total_size += 1 + 4;
    }

    // optional float ymin = 2;
    if (has_ymin()) {
      total_size += 1 + 4;
    }

    // optional float xmax = 3;
    if (has_xmax()) {
      total_size += 1 + 4;
    }

    // optional float ymax = 4;
    if (has_ymax()) {
      total_size += 1 + 4;
    }

    // optional int32 label = 5;
    if (has_label()) {
      total_size += 1 + sizeof(uint32);
    }

    // optional bool difficult = 6;
    if (has_difficult()) {
      total_size += 1 + 1;
    }

    // optional float score = 7;
    if (has_score()) {
      total_size += 1 + 4;
    }

    // optional float size = 8;
    if (has_size()) {
      total_size += 1 + 4;
    }

  }
  _cached_size_ = total_size;
  return total_size;
}


void NormalizedBBox::MergeFrom(const NormalizedBBox& from) {
 // GOOGLE_CHECK_NE(&from, this);
  if (from._has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    if (from.has_xmin()) {
      set_xmin(from.xmin());
    }
    if (from.has_ymin()) {
      set_ymin(from.ymin());
    }
    if (from.has_xmax()) {
      set_xmax(from.xmax());
    }
    if (from.has_ymax()) {
      set_ymax(from.ymax());
    }
    if (from.has_label()) {
      set_label(from.label());
    }
    if (from.has_difficult()) {
      set_difficult(from.difficult());
    }
    if (from.has_score()) {
      set_score(from.score());
    }
    if (from.has_size()) {
      set_size(from.size());
    }
  }
}


void NormalizedBBox::CopyFrom(const NormalizedBBox& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool NormalizedBBox::IsInitialized() const {

  return true;
}

void NormalizedBBox::Swap(NormalizedBBox* other) {
  if (other != this) {
    std::swap(xmin_, other->xmin_);
    std::swap(ymin_, other->ymin_);
    std::swap(xmax_, other->xmax_);
    std::swap(ymax_, other->ymax_);
    std::swap(label_, other->label_);
    std::swap(difficult_, other->difficult_);
    std::swap(score_, other->score_);
    std::swap(size_, other->size_);
    std::swap(_has_bits_[0], other->_has_bits_[0]);
    std::swap(_cached_size_, other->_cached_size_);
  }
}



}  // namespace bmcpu

