#pragma once

extern "C" {
#include <lmdb.h>
}

#include <iostream>
#include <span>
#include <string_view>
#include <system_error>

namespace LMDB
{

class Env;
class Txn;

class RuntimeError : public std::runtime_error
{
public:
  int
  code() const
  {
    return code_;
  }

private:
  RuntimeError(int code) : std::runtime_error{mdb_strerror(code)}, code_{code} {}
  int code_;
  friend void may_throw(int code);
};

inline void
may_throw(int code)
{
  if (code != MDB_SUCCESS) {
    throw RuntimeError{code};
  }
}

class Dbi
{
private:
  Dbi() {}
  MDB_dbi dbi_;
  friend class Txn;
};

class ByteSpan
{
public:
  using element_type = std::byte;
  using size_type    = std::span<const element_type>::size_type;
  using pointer      = const std::byte *;

private:
  std::span<const element_type> span_;

public:
  ByteSpan(const element_type *data, std::size_t size) : span_{data, size} {}
  ByteSpan(std::span<const element_type> span) : span_{span} {}
  ByteSpan(std::string_view sv) : span_{reinterpret_cast<const std::byte *>(sv.data()), sv.size()} {}
  ByteSpan() : span_{} {}

  constexpr pointer
  data() const noexcept
  {
    return span_.data();
  }
  constexpr size_type
  size() const noexcept
  {
    return span_.size();
  }

  operator std::span<const element_type>() const noexcept { return span_; }

  operator std::string_view() const noexcept
  {
    return std::string_view{reinterpret_cast<const char *>(span_.data()), span_.size()};
  }
};

class Val
{
private:
  Val() : val_{0, nullptr} {}
  Val(ByteSpan s)
    : val_{
        s.size(),
        const_cast<void *>(static_cast<const void *>(s.data())),
      }
  {
  }

  operator ByteSpan() const { return ByteSpan{static_cast<const std::byte *>(val_.mv_data), val_.mv_size}; }

  MDB_val val_;
  friend class Txn;
};

class Txn
{
public:
  const static unsigned int CREATE = MDB_CREATE;

  ~Txn()
  {
    if (!done_) {
      abort();
    }
  }
  Txn(const Txn &)            = delete;
  Txn &operator=(const Txn &) = delete;
  Txn(Txn &&)                 = default;
  Txn &operator=(Txn &&)      = default;

  Dbi
  open_dbi(const char *name, unsigned int flags = 0)
  {
    Dbi dbi;
    may_throw(mdb_dbi_open(txn_, name, flags, &dbi.dbi_));
    return dbi;
  }

  [[nodiscard]] bool
  may_get(Dbi dbi, ByteSpan key, ByteSpan &data)
  {
    Val key_val{key};
    Val data_val;
    int err = mdb_get(txn_, dbi.dbi_, &key_val.val_, &data_val.val_);
    if (err == MDB_NOTFOUND) {
      return false;
    }
    may_throw(err);
    data = static_cast<ByteSpan>(data_val);
    return true;
  }

  ByteSpan
  get(Dbi dbi, ByteSpan key)
  {
    Val key_val{key};
    Val data_val;
    may_throw(mdb_get(txn_, dbi.dbi_, &key_val.val_, &data_val.val_));
    return static_cast<ByteSpan>(data_val);
  }

  void
  put(Dbi dbi, ByteSpan key, ByteSpan data, unsigned int flags = 0)
  {
    Val key_val{key};
    Val data_val{data};
    may_throw(mdb_put(txn_, dbi.dbi_, &key_val.val_, &data_val.val_, flags));
  }

  void
  del(Dbi dbi, std::string_view key)
  {
    Val key_val{key};
    may_throw(mdb_del(txn_, dbi.dbi_, &key_val.val_, nullptr));
  }

  [[nodiscard]] bool
  may_del(Dbi dbi, std::string_view key)
  {
    Val key_val{key};
    int err = mdb_del(txn_, dbi.dbi_, &key_val.val_, nullptr);
    if (err == MDB_NOTFOUND) {
      return false;
    }
    may_throw(err);
    return true;
  }

  void
  commit()
  {
    may_throw(mdb_txn_commit(txn_));
    done_ = true;
  }

  void
  abort() noexcept
  {
    mdb_txn_abort(txn_);
    done_ = true;
  }

  void
  reset() noexcept
  {
    mdb_txn_reset(txn_);
    done_ = true;
  }

  void
  renew()
  {
    may_throw(mdb_txn_renew(txn_));
  }

private:
  Txn() : txn_{nullptr}, done_{false} {}
  MDB_txn *txn_;
  bool done_;
  friend class Env;
};

class Env
{
public:
  Env() { may_throw(mdb_env_create(&env_)); }

  void
  set_mapsize(size_t size)
  {
    may_throw(mdb_env_set_mapsize(env_, size));
  }

  void
  set_maxreaders(unsigned int readers)
  {
    may_throw(mdb_env_set_maxreaders(env_, readers));
  }

  void
  set_maxdbs(MDB_dbi dbs)
  {
    may_throw(mdb_env_set_maxdbs(env_, dbs));
  }

  void
  open(const char *dir_name, unsigned int flags = 0, mdb_mode_t mode = 0600)
  {
    may_throw(mdb_env_open(env_, dir_name, flags | MDB_NOTLS, mode));
  }

  Txn
  begin_txn()
  {
    return do_begin_txn();
  }

  Txn
  begin_readonly_txn()
  {
    return do_begin_txn(MDB_RDONLY);
  }

private:
  Txn
  do_begin_txn(unsigned int flags = 0)
  {
    Txn txn;
    may_throw(mdb_txn_begin(env_, nullptr, flags, &txn.txn_));
    return txn;
  }

  MDB_env *env_;
};

} // namespace LMDB
