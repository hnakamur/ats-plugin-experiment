#pragma once

extern "C" {
#include <lmdb.h>
}

#include <iostream>
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

class Val
{
private:
  Val() : val_{0, nullptr} {}
  Val(std::string_view sv)
    : val_{
        sv.size(),
        const_cast<void *>(static_cast<const void *>(sv.data())),
      }
  {
  }

  std::string_view
  to_string_view() const
  {
    return std::string_view{static_cast<const char *>(val_.mv_data), val_.mv_size};
  }

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

  [[must_use]] bool
  may_get(Dbi dbi, std::string_view key, std::string_view &data)
  {
    Val key_val{key};
    Val data_val;
    int err = mdb_get(txn_, dbi.dbi_, &key_val.val_, &data_val.val_);
    if (err == MDB_NOTFOUND) {
      return false;
    }
    may_throw(err);
    data = data_val.to_string_view();
    return true;
  }

  std::string_view
  get(Dbi dbi, std::string_view key)
  {
    Val key_val{key};
    Val data_val;
    may_throw(mdb_get(txn_, dbi.dbi_, &key_val.val_, &data_val.val_));
    return data_val.to_string_view();
  }

  void
  put(Dbi dbi, std::string_view key, std::string_view data, unsigned int flags = 0)
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

  [[must_use]] bool
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
