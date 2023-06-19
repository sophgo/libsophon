
#ifndef NORMALIZEDBBOX__INCLUDED
#define NORMALIZEDBBOX__INCLUDED
#include <string>
#include <sstream>
#include <ostream>
#include <iostream>
using namespace std;
typedef unsigned int uint32;
typedef int int32;

namespace bmcpu {

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
class Voidify {
 public:
  Voidify() {}
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(const std::ostream& s) {}
};

// Log only if condition is met.  Otherwise evaluates to void.
#define BM_FATAL_IF(condition)            \
  condition ? (void)0                  \
            : Voidify() & \
          ::cout
#define BM_CHECK_OP(val1, val2, op) \
  BM_FATAL_IF((val1 op val2)) << "Check failed: " #val1 " " #op " " #val2 " " << endl

// Check_op macro definitions
#define BM_CHECK_EQ(val1, val2) BM_CHECK_OP(val1, val2, ==)
#define BM_CHECK_NE(val1, val2) BM_CHECK_OP(val1, val2, !=)
#define BM_CHECK_LE(val1, val2) BM_CHECK_OP(val1, val2, <=)
#define BM_CHECK_LT(val1, val2) BM_CHECK_OP(val1, val2, <)
#define BM_CHECK_GE(val1, val2) BM_CHECK_OP(val1, val2, >=)
#define BM_CHECK_GT(val1, val2) BM_CHECK_OP(val1, val2, >)

const int INFO = 0;            
const int WARNING = 1;        
const int ERROR = 2;         
const int FATAL = 3;         
const int NUM_SEVERITIES = 4; 

class SSLogger {
	public:
		SSLogger(int severity) {
			switch(severity) {
			case INFO:
				stream_ << "INFO ";
				break;
			case WARNING:
				stream_ << "WARNING ";
				break;
			case ERROR:
				stream_ << "ERROR ";
				break;
			case FATAL:
				stream_ << "FATAL ";
				break;
			default:
				stream_ << "FATAL ";
			}
		}
		~SSLogger(){
			cout << stream_.str()<<endl;
		}
		std::stringstream& stream() {
			return stream_;
		}
	private:
		std::stringstream stream_;
		int severity_;
};

#define _BM_LOG_INFO \
	SSLogger(0)
#define _BM_LOG_WARNING \
	SSLogger(1)
#define _BM_LOG_ERROR \
	SSLogger(2)
#define _BM_LOG_FATAL \
	SSLogger(3)

#define BM_LOG(severity) (_BM_LOG_##severity).stream()

class NormalizedBBox;

// ===================================================================

class NormalizedBBox  {
 public:
  NormalizedBBox();
  virtual ~NormalizedBBox();

  NormalizedBBox(const NormalizedBBox& from);

  inline NormalizedBBox& operator=(const NormalizedBBox& from) {
    CopyFrom(from);
    return *this;
  }

  void Swap(NormalizedBBox* other);

  // implements Message ----------------------------------------------

  NormalizedBBox* New() const;
  void CopyFrom(const NormalizedBBox& from);
  void MergeFrom(const NormalizedBBox& from);
  void Clear();
  bool IsInitialized() const;

  int ByteSize() const;
  int GetCachedSize() const { return _cached_size_; }
  private:
  void SharedCtor();
  void SharedDtor();
  void SetCachedSize(int size) const;
  public:


  // optional float xmin = 1;
  inline bool has_xmin() const;
  inline void clear_xmin();
  static const int kXminFieldNumber = 1;
  inline float xmin() const;
  inline void set_xmin(float value);

  // optional float ymin = 2;
  inline bool has_ymin() const;
  inline void clear_ymin();
  static const int kYminFieldNumber = 2;
  inline float ymin() const;
  inline void set_ymin(float value);

  // optional float xmax = 3;
  inline bool has_xmax() const;
  inline void clear_xmax();
  static const int kXmaxFieldNumber = 3;
  inline float xmax() const;
  inline void set_xmax(float value);

  // optional float ymax = 4;
  inline bool has_ymax() const;
  inline void clear_ymax();
  static const int kYmaxFieldNumber = 4;
  inline float ymax() const;
  inline void set_ymax(float value);

  // optional int32 label = 5;
  inline bool has_label() const;
  inline void clear_label();
  static const int kLabelFieldNumber = 5;
  inline int32 label() const;
  inline void set_label(int32 value);

  // optional bool difficult = 6;
  inline bool has_difficult() const;
  inline void clear_difficult();
  static const int kDifficultFieldNumber = 6;
  inline bool difficult() const;
  inline void set_difficult(bool value);

  // optional float score = 7;
  inline bool has_score() const;
  inline void clear_score();
  static const int kScoreFieldNumber = 7;
  inline float score() const;
  inline void set_score(float value);

  // optional float size = 8;
  inline bool has_size() const;
  inline void clear_size();
  static const int kSizeFieldNumber = 8;
  inline float size() const;
  inline void set_size(float value);

 private:
  inline void set_has_xmin();
  inline void clear_has_xmin();
  inline void set_has_ymin();
  inline void clear_has_ymin();
  inline void set_has_xmax();
  inline void clear_has_xmax();
  inline void set_has_ymax();
  inline void clear_has_ymax();
  inline void set_has_label();
  inline void clear_has_label();
  inline void set_has_difficult();
  inline void clear_has_difficult();
  inline void set_has_score();
  inline void clear_has_score();
  inline void set_has_size();
  inline void clear_has_size();

 
  uint32 _has_bits_[1];
  mutable int _cached_size_;
  float xmin_;
  float ymin_;
  float xmax_;
  float ymax_;
  int32 label_;
  bool difficult_;
  float score_;
  float size_;
};
// ===================================================================


// ===================================================================

// NormalizedBBox

// optional float xmin = 1;
inline bool NormalizedBBox::has_xmin() const {
  return (_has_bits_[0] & 0x00000001u) != 0;
}
inline void NormalizedBBox::set_has_xmin() {
  _has_bits_[0] |= 0x00000001u;
}
inline void NormalizedBBox::clear_has_xmin() {
  _has_bits_[0] &= ~0x00000001u;
}
inline void NormalizedBBox::clear_xmin() {
  xmin_ = 0;
  clear_has_xmin();
}
inline float NormalizedBBox::xmin() const {
  return xmin_;
}
inline void NormalizedBBox::set_xmin(float value) {
  set_has_xmin();
  xmin_ = value;
}

// optional float ymin = 2;
inline bool NormalizedBBox::has_ymin() const {
  return (_has_bits_[0] & 0x00000002u) != 0;
}
inline void NormalizedBBox::set_has_ymin() {
  _has_bits_[0] |= 0x00000002u;
}
inline void NormalizedBBox::clear_has_ymin() {
  _has_bits_[0] &= ~0x00000002u;
}
inline void NormalizedBBox::clear_ymin() {
  ymin_ = 0;
  clear_has_ymin();
}
inline float NormalizedBBox::ymin() const {
  return ymin_;
}
inline void NormalizedBBox::set_ymin(float value) {
  set_has_ymin();
  ymin_ = value;
}

// optional float xmax = 3;
inline bool NormalizedBBox::has_xmax() const {
  return (_has_bits_[0] & 0x00000004u) != 0;
}
inline void NormalizedBBox::set_has_xmax() {
  _has_bits_[0] |= 0x00000004u;
}
inline void NormalizedBBox::clear_has_xmax() {
  _has_bits_[0] &= ~0x00000004u;
}
inline void NormalizedBBox::clear_xmax() {
  xmax_ = 0;
  clear_has_xmax();
}
inline float NormalizedBBox::xmax() const {
  return xmax_;
}
inline void NormalizedBBox::set_xmax(float value) {
  set_has_xmax();
  xmax_ = value;
}

// optional float ymax = 4;
inline bool NormalizedBBox::has_ymax() const {
  return (_has_bits_[0] & 0x00000008u) != 0;
}
inline void NormalizedBBox::set_has_ymax() {
  _has_bits_[0] |= 0x00000008u;
}
inline void NormalizedBBox::clear_has_ymax() {
  _has_bits_[0] &= ~0x00000008u;
}
inline void NormalizedBBox::clear_ymax() {
  ymax_ = 0;
  clear_has_ymax();
}
inline float NormalizedBBox::ymax() const {
  return ymax_;
}
inline void NormalizedBBox::set_ymax(float value) {
  set_has_ymax();
  ymax_ = value;
}

// optional int32 label = 5;
inline bool NormalizedBBox::has_label() const {
  return (_has_bits_[0] & 0x00000010u) != 0;
}
inline void NormalizedBBox::set_has_label() {
  _has_bits_[0] |= 0x00000010u;
}
inline void NormalizedBBox::clear_has_label() {
  _has_bits_[0] &= ~0x00000010u;
}
inline void NormalizedBBox::clear_label() {
  label_ = 0;
  clear_has_label();
}
inline int32 NormalizedBBox::label() const {
  return label_;
}
inline void NormalizedBBox::set_label(int32 value) {
  set_has_label();
  label_ = value;
}

// optional bool difficult = 6;
inline bool NormalizedBBox::has_difficult() const {
  return (_has_bits_[0] & 0x00000020u) != 0;
}
inline void NormalizedBBox::set_has_difficult() {
  _has_bits_[0] |= 0x00000020u;
}
inline void NormalizedBBox::clear_has_difficult() {
  _has_bits_[0] &= ~0x00000020u;
}
inline void NormalizedBBox::clear_difficult() {
  difficult_ = false;
  clear_has_difficult();
}
inline bool NormalizedBBox::difficult() const {
  return difficult_;
}
inline void NormalizedBBox::set_difficult(bool value) {
  set_has_difficult();
  difficult_ = value;
}

// optional float score = 7;
inline bool NormalizedBBox::has_score() const {
  return (_has_bits_[0] & 0x00000040u) != 0;
}
inline void NormalizedBBox::set_has_score() {
  _has_bits_[0] |= 0x00000040u;
}
inline void NormalizedBBox::clear_has_score() {
  _has_bits_[0] &= ~0x00000040u;
}
inline void NormalizedBBox::clear_score() {
  score_ = 0;
  clear_has_score();
}
inline float NormalizedBBox::score() const {
  return score_;
}
inline void NormalizedBBox::set_score(float value) {
  set_has_score();
  score_ = value;
}

// optional float size = 8;
inline bool NormalizedBBox::has_size() const {
  return (_has_bits_[0] & 0x00000080u) != 0;
}
inline void NormalizedBBox::set_has_size() {
  _has_bits_[0] |= 0x00000080u;
}
inline void NormalizedBBox::clear_has_size() {
  _has_bits_[0] &= ~0x00000080u;
}
inline void NormalizedBBox::clear_size() {
  size_ = 0;
  clear_has_size();
}
inline float NormalizedBBox::size() const {
  return size_;
}
inline void NormalizedBBox::set_size(float value) {
  set_has_size();
  size_ = value;
}



}  // namespace bmcpu


#endif  // BMCPU__INCLUDED
