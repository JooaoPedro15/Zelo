#include "collectors/snapshot_collector.hpp"

#include "collectors/startup_collector.hpp"
#include "collectors/system_paths.hpp"
#include "collectors/temporary_files_collector.hpp"
#include "collectors/volume_collector.hpp"

namespace zelo::collectors {

core::SystemSnapshot collect_snapshot(std::stop_token token) {
    core::SystemSnapshot snapshot;

    VolumeCollector{}.collect_into(snapshot);
    StartupCollector{}.collect_into(snapshot);

    const TemporaryFilesCollector temporary_files{build_protected_paths(collect_system_paths())};
    temporary_files.collect_into(snapshot, token);

    return snapshot;
}

}
