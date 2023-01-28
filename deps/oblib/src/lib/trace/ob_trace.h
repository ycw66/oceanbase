/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#ifndef _OB_TRACE_H
#define _OB_TRACE_H

#include "ob_trace_def.h"
#include "lib/list/ob_dlist.h"
#include "lib/utility/utility.h"

#define MAX_TRACE_LOG_SIZE 8 * 1024 // 8K

#define SET_TRACE_BUFFER(buffer, size) OBTRACE->set_trace_buffer(buffer, size)
#define FLT_BEGIN_TRACE() (OBTRACE->begin())
#define FLT_END_TRACE() (OBTRACE->end())
#define FLT_BEGIN_SPAN(span_type) FLT_BEGIN_CHILD_SPAN(span_type)
#define FLT_BEGIN_CHILD_SPAN(span_type) (OBTRACE->begin_span(::oceanbase::trace::ObSpanType::span_type, GET_SPANLEVEL(::oceanbase::trace::ObSpanType::span_type), false))
#define FLT_BEGIN_FOLLOW_SPAN(span_type) (OBTRACE->begin_span(::oceanbase::trace::ObSpanType::span_type, GET_SPANLEVEL(::oceanbase::trace::ObSpanType::span_type), true))
#define FLT_END_SPAN(span)                                      \
if (OB_NOT_NULL(span)) {                                        \
  OBTRACE->end_span(span);                                      \
  if (span->span_id_.is_inited() && OBTRACE->is_auto_flush()) { \
    FLUSH_TRACE();                                              \
  }                                                             \
}
#define FLT_RESET_SPAN() (OBTRACE->reset_span())
#define FLT_END_CURRENT_SPAN() FLT_END_SPAN(OBTRACE->last_active_span_)
#define FLT_SET_TAG(...) (OBTRACE->set_tag(__VA_ARGS__))
#define FLT_SET_LOG()
#define FLT_SET_TRACE_LEVEL(level) (OBTRACE->set_level(level))
#define FLT_SET_AUTO_FLUSH(value) (OBTRACE->set_auto_flush(value))

#define FLT_RESTORE_DDL_TRACE_CTX(flt_ctx) (OBTRACE->init(flt_ctx))
#define FLT_RESTORE_DDL_SPAN(span_type, span_id, start_ts) (OBTRACE->begin_span_by_id(::oceanbase::trace::ObSpanType::span_type, GET_SPANLEVEL(::oceanbase::trace::ObSpanType::span_type), false, span_id, start_ts))
#define FLT_RELEASE_DDL_SPAN(span) (OBTRACE->release_span(span))

#define FLUSH_TRACE() ::oceanbase::trace::flush_trace();

#define FLTSpanGuard(span_type) ::oceanbase::trace::__ObFLTSpanGuard __##span_type##__LINE__(::oceanbase::trace::ObSpanType::span_type, GET_SPANLEVEL(::oceanbase::trace::ObSpanType::span_type))

#define OBTRACE ::oceanbase::trace::ObTrace::get_instance()

namespace oceanbase
{
namespace trace
{
static const char* __span_type_mapper[] = {
#define FLT_DEF_SPAN(name, comment) #name,
#define __HIGH_LEVEL_SPAN
#define __MIDDLE_LEVEL_SPAN
#define __LOW_LEVEL_SPAN
#include "lib/trace/ob_trace_def.h"
#undef __LOW_LEVEL_SPAN
#undef __MIDDLE_LEVEL_SPAN
#undef __HIGH_LEVEL_SPAN
#undef FLT_DEF_SPAN
};

extern void flush_trace();

struct UUID
{
  static UUID gen();
  static uint64_t gen_rand();
  UUID() : low_(0), high_(0) {}
  explicit UUID(const char* uuid);
  bool equal(const UUID& that) const { return 0 == memcmp(this, &that, sizeof(UUID)); }
  OB_INLINE bool is_inited() const { return low_ != 0 || high_ != 0; }
  int tostring(char* buf, const int64_t buf_len, int64_t& pos) const;
  int serialize(char* buf, const int64_t buf_len, int64_t& pos) const;
  int deserialize(const char* buf, const int64_t buf_len, int64_t& pos);
  int64_t get_serialize_size() const { return 16; }
  int64_t to_string(char* buf, const int64_t buf_len) const
  {
    int64_t pos = 0;
    int ret = tostring(buf, buf_len, pos);
    if (OB_FAIL(ret)) {
      pos = 0;
    }
    return pos;
  }
  union {
    struct {
      uint64_t low_;
      uint64_t high_;
    };
    struct {
      uint32_t time_low;
      uint16_t time_mid;
      uint16_t time_hi_and_version;
      uint8_t clock_seq_hi_and_reserved;
      uint8_t clock_seq_low;
      uint8_t node[6];
    };
  };
};

struct FltTransCtx {
  OB_UNIS_VERSION(1);
public:
  FltTransCtx()
    : trace_id_(), span_id_(), policy_(0)
  {}
  TO_STRING_KV(K_(trace_id), K_(span_id), K_(level), K_(auto_flush));

public:
  UUID trace_id_;
  UUID span_id_;
  union {
    uint8_t policy_;
    struct {
      uint8_t level_ : 7;
      bool auto_flush_ : 1;
    };
  };
};

struct ObTagCtxBase
{
  ObTagCtxBase() : next_(nullptr), tag_type_(0) {}
  virtual ~ObTagCtxBase() {}
  virtual int tostring(char* buf, const int64_t buf_len, int64_t& pos)
  {
    int ret = OB_SUCCESS;
    const auto l = strlen(__tag_name_mapper[tag_type_]);
    if (pos + l + 7 >= buf_len) {
      buf[std::min(pos - 1, buf_len - 1)] = '\0';
      ret = OB_BUF_NOT_ENOUGH;
    } else {
      buf[pos++] = '{';
      buf[pos++] = '\"';
      IGNORE_RETURN strcpy(buf + pos, __tag_name_mapper[tag_type_]);
      pos += l;
      buf[pos++] = '\"';
      buf[pos++] = ':';
      buf[pos] = '\0';
    }
    return ret;
  }
  ObTagCtxBase* next_;
  uint16_t tag_type_;
};

// if T has member to_string, add quote for it.
template <typename T>
int tag_to_string(char* buf, const int64_t buf_len, int64_t& pos, const T& value, TrueType)
{
  int ret = OB_SUCCESS;
  if (pos + value.length() + 4 >= buf_len) {
    ret = OB_BUF_NOT_ENOUGH;
  } else {
    buf[pos++] = '\"';
    ret = logdata_print_obj(buf, buf_len, pos, value);
    buf[pos++] = '\"';
    buf[pos++] = '}';
    buf[pos] = '\0';
  }
  return ret;
}

// else use default to_string
template <typename T>
int tag_to_string(char* buf, const int64_t buf_len, int64_t& pos, const T& value, FalseType)
{
  int ret = OB_SUCCESS;
  if (OB_FAIL(logdata_print_obj(buf, buf_len, pos, value))) {
    // do nothing
  } else if (pos + 1 >= buf_len) {
    ret = OB_BUF_NOT_ENOUGH;
  } else {
    buf[pos++] = '}';
    buf[pos] = '\0';
  }
  return ret;
}

template <typename T>
struct ObTagCtx final : public ObTagCtxBase
{
  ObTagCtx() {}
  virtual ~ObTagCtx() override {}
  virtual int tostring(char* buf, const int64_t buf_len, int64_t& pos) override
  {
    int ret = OB_SUCCESS;
    if (OB_FAIL(ObTagCtxBase::tostring(buf, buf_len, pos))) {
      // do nothing
    } else {
      ret = tag_to_string(buf, buf_len, pos, data_, BoolType<HAS_MEMBER(T, to_string)>());
    }
    return ret;
  }
  T data_;
};

struct ObSpanCtx final : public common::ObDLinkBase<ObSpanCtx>
{
  ObSpanCtx();

  UUID span_id_;
  ObSpanCtx* source_span_;
  int64_t start_ts_;
  int64_t end_ts_;
  ObTagCtxBase* tags_;
  uint16_t span_type_;
  bool is_follow_;
};

struct ObTrace
{
  static constexpr uint64_t MAGIC_CODE = 0x1234567887654321ul;
  static constexpr int64_t DEFAULT_BUFFER_SIZE = (1L << 16);
  static constexpr int64_t MIN_BUFFER_SIZE = (1L << 13);
  static ObTrace* get_instance();
  static void set_trace_buffer(void* buffer, int64_t buffer_size);
  ObTrace(int64_t buffer_size);
  void init(FltTransCtx &flt_ctx)
  {
    root_span_id_ = flt_ctx.span_id_;
    trace_id_ = flt_ctx.trace_id_;
    policy_ = flt_ctx.policy_;
  }
  void init(UUID trace_id, UUID root_span_id, uint8_t policy = 0);
  bool is_inited() { return check_magic() && trace_id_.is_inited(); }
  UUID begin();
  void end();
  ObSpanCtx* begin_span(uint32_t span_type, uint8_t level, bool is_follow);
  // used in ddl task tracing
  ObSpanCtx* begin_span_by_id(const uint32_t span_type,
                              const uint8_t level,
                              const bool is_follow,
                              const UUID span_id,
                              const int64_t start_ts);
  void release_span(ObSpanCtx *&span);
  void end_span(ObSpanCtx* span_id);
  void reset_span();
  template <typename T, typename... Targs>
  void set_tag(ObTagType tag_type, const T& value, Targs... Fargs)
  {
    if (trace_id_.is_inited()
        && OB_NOT_NULL(last_active_span_)
        && GET_TAGLEVEL(tag_type) <= level_) {
      if (OB_UNLIKELY(!append_tag(tag_type, value))) {
        FLUSH_TRACE();
        IGNORE_RETURN append_tag(tag_type, value);
      }
      set_tag(std::forward<Targs>(Fargs)...);
    }
  }
  int serialize(char* buf, const int64_t buf_len, int64_t& pos) const;
  int deserialize(const char* buf, const int64_t buf_len, int64_t& pos);
  int64_t get_serialize_size() const { return 33; }
  OB_INLINE UUID get_trace_id() { return trace_id_; }
  OB_INLINE UUID get_root_span_id() { return root_span_id_; }
  OB_INLINE uint8_t get_policy() { return policy_; }
  OB_INLINE uint8_t get_level() { return level_; }
  OB_INLINE void set_level(uint8_t level) { level_ = 0x7f & level; }
  OB_INLINE void set_auto_flush(bool auto_flush) { auto_flush_ = auto_flush; }
  OB_INLINE bool is_auto_flush() { return auto_flush_; }
  void check_leak_span();
  void reset();
  void dump_span();
  template<class T, typename std::enable_if<!std::is_same<T, ObString>::value && !std::is_convertible<T, const char*>::value, bool>::type = true>
  bool append_tag(ObTagType tag_type, const T& value)
  {
    int ret = false;
    if (offset_ + sizeof(ObTagCtx<T>) >= buffer_size_) {
      // do nothing
    } else {
      ObTagCtx<T>* tag = new (data_ + offset_) ObTagCtx<T>;
      tag->next_ = last_active_span_->tags_;
      last_active_span_->tags_ = tag;
      tag->tag_type_ = tag_type;
      tag->data_ = value;
      offset_ += sizeof(ObTagCtx<T>);
      ret = true;
    }
    return ret;
  }
  template<class T, typename std::enable_if<std::is_same<T, ObString>::value, bool>::type = true>
  bool append_tag(ObTagType tag_type, const T& value)
  {
    int ret = false;
    ObString v("");
    if (OB_ISNULL(value.ptr())) {
      // do nothing
    } else {
      v = value;
    }
    auto l = v.length();
    if (offset_ + sizeof(ObTagCtx<void*>) + l + 1 - sizeof(void*) >= buffer_size_) {
      // do nothing
    } else {
      ObTagCtx<void*>* tag = new (data_ + offset_) ObTagCtx<void*>;
      tag->next_ = last_active_span_->tags_;
      last_active_span_->tags_ = tag;
      tag->tag_type_ = tag_type;
      memcpy(&(tag->data_), v.ptr(), l);
      offset_ += (sizeof(ObTagCtx<void*>) + l + 1 - sizeof(void*));
      data_[offset_ - 1] = '\0';
      ret = true;
    }
    return ret;
  }
  template<class T, typename std::enable_if<std::is_convertible<T, const char*>::value, bool>::type = true>
  bool append_tag(ObTagType tag_type, const T& value)
  {
    return append_tag(tag_type, OB_ISNULL(value) ? ObString("") : ObString(value));
  }
private:
  bool check_magic() { return MAGIC_CODE == magic_code_; }
  void set_tag() {}
private:
  static thread_local ObTrace* save_buffer;
  uint64_t magic_code_;
public:
  int64_t buffer_size_;
  int64_t offset_;
  common::ObDList<ObSpanCtx> current_span_;
  common::ObDList<ObSpanCtx> freed_span_;
  ObSpanCtx* last_active_span_;
private:
  UUID trace_id_;
  UUID root_span_id_;
  union {
    uint8_t policy_;
    struct {
      uint8_t level_ : 7;
      bool auto_flush_ : 1;
    };
  };
  uint64_t seq_;
  char data_[0];
};

class __ObFLTSpanGuard
{
public:
  __ObFLTSpanGuard(uint32_t span_type, uint8_t level)
  {
    span_ = OBTRACE->begin_span(span_type, level, false);
#ifndef NDEBUG
    if (OB_NOT_NULL(span_) && span_->span_id_.is_inited()) {
      FLT_SET_TAG(span_back_trace, lbt());
    }
#endif
  }
  ~__ObFLTSpanGuard() { FLT_END_SPAN(span_); }
private:
  ObSpanCtx* span_;
};

template<>
int ObTagCtx<char*>::tostring(char* buf, const int64_t buf_len, int64_t& pos);

} // trace
} // oceanbase

namespace oceanbase
{
namespace sql
{
class ObFLTSpanMgr;
extern ObFLTSpanMgr* get_flt_span_manager();
extern int handle_span_record(char* buf, const int64_t buf_len, ObFLTSpanMgr* flt_span_manager, ::oceanbase::trace::ObSpanCtx* span);
}
}

#endif /* _OB_TRACE_H */
