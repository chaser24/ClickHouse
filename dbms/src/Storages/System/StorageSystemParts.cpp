#include "StorageSystemParts.h"

#include <Common/escapeForFileName.h>
#include <Columns/ColumnString.h>
#include <DataTypes/DataTypeString.h>
#include <DataTypes/DataTypesNumber.h>
#include <DataTypes/DataTypeDateTime.h>
#include <DataTypes/DataTypeDate.h>
#include <DataStreams/OneBlockInputStream.h>
#include <Storages/VirtualColumnUtils.h>
#include <Databases/IDatabase.h>
#include <Common/hex.h>

namespace DB
{

StorageSystemParts::StorageSystemParts(const std::string & name_)
    : StorageSystemPartsBase(name_,
    {
        {"partition",                                  std::make_shared<DataTypeString>()},
        {"name",                                       std::make_shared<DataTypeString>()},
        {"active",                                     std::make_shared<DataTypeUInt8>()},
        {"marks",                                      std::make_shared<DataTypeUInt64>()},
        {"rows",                                       std::make_shared<DataTypeUInt64>()},
        {"bytes_on_disk",                              std::make_shared<DataTypeUInt64>()},
        {"data_compressed_bytes",                      std::make_shared<DataTypeUInt64>()},
        {"data_uncompressed_bytes",                    std::make_shared<DataTypeUInt64>()},
        {"marks_bytes",                                std::make_shared<DataTypeUInt64>()},
        {"modification_time",                          std::make_shared<DataTypeDateTime>()},
        {"remove_time",                                std::make_shared<DataTypeDateTime>()},
        {"refcount",                                   std::make_shared<DataTypeUInt32>()},
        {"min_date",                                   std::make_shared<DataTypeDate>()},
        {"max_date",                                   std::make_shared<DataTypeDate>()},
        {"min_time",                                   std::make_shared<DataTypeDateTime>()},
        {"max_time",                                   std::make_shared<DataTypeDateTime>()},
        {"partition_id",                               std::make_shared<DataTypeString>()},
        {"min_block_number",                           std::make_shared<DataTypeInt64>()},
        {"max_block_number",                           std::make_shared<DataTypeInt64>()},
        {"level",                                      std::make_shared<DataTypeUInt32>()},
        {"data_version",                               std::make_shared<DataTypeUInt64>()},
        {"primary_key_bytes_in_memory",                std::make_shared<DataTypeUInt64>()},
        {"primary_key_bytes_in_memory_allocated",      std::make_shared<DataTypeUInt64>()},
        {"is_frozen",                                  std::make_shared<DataTypeUInt8>()},

        {"database",                                   std::make_shared<DataTypeString>()},
        {"table",                                      std::make_shared<DataTypeString>()},
        {"engine",                                     std::make_shared<DataTypeString>()},
        {"path",                                       std::make_shared<DataTypeString>()},

        {"hash_of_all_files",                          std::make_shared<DataTypeString>()},
        {"hash_of_uncompressed_files",                 std::make_shared<DataTypeString>()},
        {"uncompressed_hash_of_compressed_files",      std::make_shared<DataTypeString>()}
    }
    )
{
}

void StorageSystemParts::processNextStorage(MutableColumns & columns_, const StoragesInfo & info, bool has_state_column)
{
    using State = MergeTreeDataPart::State;
    MergeTreeData::DataPartStateVector all_parts_state;
    MergeTreeData::DataPartsVector all_parts;

    all_parts = info.getParts(all_parts_state, has_state_column);

    for (size_t part_number = 0; part_number < all_parts.size(); ++part_number)
    {
        const auto & part = all_parts[part_number];
        auto part_state = all_parts_state[part_number];

        ColumnSize columns_size = part->getTotalColumnsSize();

        size_t i = 0;
        {
            WriteBufferFromOwnString out;
            part->partition.serializeText(*info.data, out, format_settings);
            columns_[i++]->insert(out.str());
        }
        columns_[i++]->insert(part->name);
        columns_[i++]->insert(part_state == State::Committed);
        columns_[i++]->insert(part->getMarksCount());
        columns_[i++]->insert(part->rows_count);
        columns_[i++]->insert(part->bytes_on_disk.load(std::memory_order_relaxed));
        columns_[i++]->insert(columns_size.data_compressed);
        columns_[i++]->insert(columns_size.data_uncompressed);
        columns_[i++]->insert(columns_size.marks);
        columns_[i++]->insert(static_cast<UInt64>(part->modification_time));

        time_t remove_time = part->remove_time.load(std::memory_order_relaxed);
        columns_[i++]->insert(static_cast<UInt64>(remove_time == std::numeric_limits<time_t>::max() ? 0 : remove_time));

        /// For convenience, in returned refcount, don't add references that was due to local variables in this method: all_parts, active_parts.
        columns_[i++]->insert(static_cast<UInt64>(part.use_count() - 1));

        columns_[i++]->insert(part->getMinDate());
        columns_[i++]->insert(part->getMaxDate());
        columns_[i++]->insert(part->getMinTime());
        columns_[i++]->insert(part->getMaxTime());
        columns_[i++]->insert(part->info.partition_id);
        columns_[i++]->insert(part->info.min_block);
        columns_[i++]->insert(part->info.max_block);
        columns_[i++]->insert(part->info.level);
        columns_[i++]->insert(static_cast<UInt64>(part->info.getDataVersion()));
        columns_[i++]->insert(part->getIndexSizeInBytes());
        columns_[i++]->insert(part->getIndexSizeInAllocatedBytes());
        columns_[i++]->insert(part->is_frozen);

        columns_[i++]->insert(info.database);
        columns_[i++]->insert(info.table);
        columns_[i++]->insert(info.engine);
        columns_[i++]->insert(part->getFullPath());

        if (has_state_column)
            columns_[i++]->insert(part->stateString());

        MinimalisticDataPartChecksums helper;
        {
            /// TODO: MergeTreeDataPart structure is too error-prone.
            std::shared_lock<std::shared_mutex> lock(part->columns_lock);
            helper.computeTotalChecksums(part->checksums);
        }

        auto checksum = helper.hash_of_all_files;
        columns_[i++]->insert(getHexUIntLowercase(checksum.first) + getHexUIntLowercase(checksum.second));

        checksum = helper.hash_of_uncompressed_files;
        columns_[i++]->insert(getHexUIntLowercase(checksum.first) + getHexUIntLowercase(checksum.second));

        checksum = helper.uncompressed_hash_of_compressed_files;
        columns_[i++]->insert(getHexUIntLowercase(checksum.first) + getHexUIntLowercase(checksum.second));
    }
}

}
