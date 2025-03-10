#pragma once

#include <string>
#include <map>
#include <vector>

#include "../../src/redis_db.h"
#include "../../src/status.h"
#include "../../src/storage.h"
#include "../../src/redis_metadata.h"
#include "../../src/redis_slot.h"

#include "config.h"
#include "writer.h"

class LatestSnapShot {
 public:
  explicit LatestSnapShot(rocksdb::DB *db) : db_(db) {
    snapshot_ = db_->GetSnapshot();
  }
  ~LatestSnapShot() {
    db_->ReleaseSnapshot(snapshot_);
  }
  const rocksdb::Snapshot *GetSnapShot() { return snapshot_; }
 private:
  rocksdb::DB *db_ = nullptr;
  const rocksdb::Snapshot *snapshot_ = nullptr;
};

class Parser {
 public:
  explicit Parser(Engine::Storage *storage, Writer *writer)
      : storage_(storage), writer_(writer) {
    lastest_snapshot_ = new LatestSnapShot(storage->GetDB());
    cluster_enabled_ = storage_->IsSlotIdEncoded();
  }
  ~Parser() {
    delete lastest_snapshot_;
    lastest_snapshot_ = nullptr;
  }

  Status ParseFullDB();
  rocksdb::Status ParseWriteBatch(const std::string &batch_string);

 protected:
  Engine::Storage *storage_ = nullptr;
  Writer *writer_ = nullptr;
  LatestSnapShot *lastest_snapshot_ = nullptr;
  bool cluster_enabled_ = false;

  Status parseSimpleKV(const Slice &ns_key, const Slice &value, int expire);
  Status parseComplexKV(const Slice &ns_key, const Metadata &metadata);
  Status parseBitmapSegment(const Slice &ns, const Slice &user_key, int index, const Slice &bitmap);
};

// An extractor to extract update from raw writebatch
class WriteBatchExtractor : public rocksdb::WriteBatch::Handler {
 public:
  WriteBatchExtractor() {}
  explicit WriteBatchExtractor(bool cluster_enabled, int16_t slot = -1, bool to_redis = true)
  : cluster_enabled_(cluster_enabled), slot_(slot), to_redis_(to_redis) {}
  void LogData(const rocksdb::Slice &blob) override;
  rocksdb::Status PutCF(uint32_t column_family_id, const Slice &key,
                        const Slice &value) override;

  rocksdb::Status DeleteCF(uint32_t column_family_id, const Slice &key) override;
  rocksdb::Status DeleteRangeCF(uint32_t column_family_id,
                                const Slice& begin_key, const Slice& end_key) override;
  std::map<std::string, std::vector<std::string>> *GetAofStrings() { return &aof_strings_; }
 private:
  std::map<std::string, std::vector<std::string>> aof_strings_;
  Redis::WriteBatchLogData log_data_;
  bool firstSeen_ = true;
  bool cluster_enabled_ = false;
  int slot_;
  bool to_redis_;
};
