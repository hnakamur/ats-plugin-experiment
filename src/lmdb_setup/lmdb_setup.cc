#include <iostream>
#include <filesystem>

#include <yaml-cpp/yaml.h>
#include "lmdb-cpp.h"

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

    try {
      std::filesystem::create_directories(lmdb_path);
      LMDB::Env env;
      env.set_mapsize(map_size);
      env.set_maxreaders(max_readers);
      env.set_maxdbs(max_dbs);
      env.open(lmdb_path.c_str());
      auto txn = env.begin_txn();
      auto dbi = txn.open_dbi("credentials", LMDB::Txn::CREATE);

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
        txn.put(dbi, key, value);
      }
      txn.commit();
    } catch (const LMDB::RuntimeError &e) {
      std::cerr << e.what() << " while using LMDB database " << lmdb_path << '\n';
      return 1;
    }
  } catch (const YAML::Exception &e) {
    std::cerr << e.what() << " while parsing YAML config file " << config_path << '\n';
    return 1;
  }
}
