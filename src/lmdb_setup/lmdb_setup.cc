#include <iostream>
#include <filesystem>

#include <yaml-cpp/yaml.h>
#include <lmdb.h>

int
main(int argc, char **argv)
{
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " /path/to/obj_store_auth.yaml\n";
    return 2;
  }

  auto config_path = argv[1];
  try {
    YAML::Node config           = YAML::LoadFile(config_path);
    const std::string lmdb_path = config["lmdb_path"].as<std::string>();
    const int map_size          = config["map_size"].as<int>();
    const int max_readers       = config["max_readers"].as<int>();
    const int max_dbs           = config["max_dbs"].as<int>();

    std::filesystem::create_directories(lmdb_path);
    MDB_env *env;
    int err = mdb_env_create(&env);
    if (err != MDB_SUCCESS) {
      std::cerr << "mdb_env_create error: " << mdb_strerror(err) << "\n";
      return 1;
    }
    err = mdb_env_set_mapsize(env, map_size);
    if (err != MDB_SUCCESS) {
      std::cerr << "mdb_env_set_mapsize error: " << mdb_strerror(err) << "\n";
      return 1;
    }
    err = mdb_env_set_maxreaders(env, max_readers);
    if (err != MDB_SUCCESS) {
      std::cerr << "mdb_env_set_maxreaders error: " << mdb_strerror(err) << "\n";
      return 1;
    }
    err = mdb_env_set_maxdbs(env, max_dbs);
    if (err != MDB_SUCCESS) {
      std::cerr << "mdb_env_set_maxdbs error: " << mdb_strerror(err) << "\n";
      return 1;
    }
    err = mdb_env_open(env, lmdb_path.c_str(), MDB_NOTLS, 0755);
    if (err != MDB_SUCCESS) {
      std::cerr << "mdb_env_open error: " << mdb_strerror(err) << "\n";
      return 1;
    }
    MDB_txn *txn;
    err = mdb_txn_begin(env, nullptr, 0, &txn);
    if (err != MDB_SUCCESS) {
      std::cerr << "mdb_txn_begin error: " << mdb_strerror(err) << "\n";
      return 1;
    }
    MDB_dbi dbi;
    err = mdb_dbi_open(txn, "credentials", MDB_CREATE, &dbi);
    if (err != MDB_SUCCESS) {
      std::cerr << "mdb_dbi_open error: " << mdb_strerror(err) << "\n";
      mdb_txn_abort(txn);
      return 1;
    }

    YAML::Node credentials = config["credentials"];
    for (YAML::const_iterator it = credentials.begin(); it != credentials.end(); ++it) {
      auto credential = *it;
      auto key        = credential["key"].as<std::string>();
      auto access_key = credential["access_key"].as<std::string>();
      auto secret_key = credential["secret_key"].as<std::string>();
      auto bucket     = credential["bucket"].as<std::string>();
      auto endpoint   = credential["endpoint"].as<std::string>();
      auto region     = credential["region"].as<std::string>();
      std::ostringstream value_stream;
      value_stream << bucket << '\t' << endpoint << '\t' << region << '\t' << access_key << '\t' << secret_key;
      auto value = value_stream.str();

      MDB_val mdb_key;
      MDB_val mdb_data;
      mdb_key.mv_size  = key.size();
      mdb_key.mv_data  = key.data();
      mdb_data.mv_size = value.size();
      mdb_data.mv_data = value.data();
      err              = mdb_put(txn, dbi, &mdb_key, &mdb_data, 0);
      if (err != MDB_SUCCESS) {
        std::cerr << "mdb_put error: " << mdb_strerror(err) << "\n";
        mdb_txn_abort(txn);
        return 1;
      }
    }

    err = mdb_txn_commit(txn);
    if (err != MDB_SUCCESS) {
      std::cerr << "mdb_txn_commit error: " << mdb_strerror(err) << "\n";
      return 1;
    }
  } catch (const YAML::Exception &e) {
    std::cerr << e.what() << " while parsing YAML config file " << config_path << '\n';
    return 1;
  }
}
