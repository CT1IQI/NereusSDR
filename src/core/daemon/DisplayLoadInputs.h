// =================================================================
// src/core/daemon/DisplayLoadInputs.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Chooses which of the Core's load
// readings the display load governor may act on (R-R3-40, R-R3-41): only
// load that lowering spectrum can relieve. No upstream logic is involved.
// =================================================================

#pragma once

#include "core/platform/ThreadPlacement.h"
#include "core/session/media/DisplayLoadGovernor.h"
#include "models/ReceiverDspLoadSampler.h"

#include <QList>

#include <optional>

namespace NereusSDR {

/// Everything the Core has measured for one governor evaluation, from
/// caches only (RadioModel::receiverDspLoad, the shared host sampler, the
/// thread placement plan).
struct DisplayLoadInputs {
    struct Receiver {
        int sliceId = -1; ///< also the receiver's WDSP channel
        ReceiverDspLoad load;
    };
    QList<Receiver> receivers;
    /// All cores (the telemetry value).
    std::optional<double> systemCpuPercent;
    /// While thread placement is active, the cores displayGovernorCpus()
    /// names: the housekeeping cores, where encoding, Opus and sending
    /// run, and every core a spectrum thread may use. Where spectrum runs
    /// on signal processing cores (the Rock 5C) this includes the raised
    /// receive and transmit threads' time there, which lowering the
    /// display cannot relieve.
    std::optional<double> housekeepingCpuPercent;
    /// How long ago the host sample behind both CPU values began, in
    /// milliseconds (SharedHostSampler::cpuSampleBeganMsAgo). Absent with
    /// no CPU values.
    std::optional<qint64> cpuSampleBeganMsAgo;
    /// The thread placement now in force (ThreadPlacement::appliedPlan):
    /// only the roles whose thread really runs on its own core, so a
    /// refused move counts as sharing. Inactive when threads are not placed
    /// (placement off, priority only, one core, not Linux).
    PlacementPlan placement;
};

/// True when lowering spectrum can relieve this receiver's processing:
/// its WDSP worker or the DSP thread shares cores with spectrum and
/// networking, on the housekeeping cores, or on signal processing cores a
/// spectrum thread may use while the thread itself is not raised above it
/// (the system refused the priority). A receiver whose worker and DSP
/// thread both run raised on signal processing cores is not slowed by
/// display work, so its load is not the display's to relieve.
bool receiverSharesDisplayCores(const PlacementPlan& placement, int sliceId);

/// The cores whose CPU share the governor reads (SharedHostSampler::
/// setGovernorCpus): the housekeeping cores and every core a spectrum
/// thread may use, ascending, each once. Empty, meaning all cores, when
/// threads are not placed.
QList<int> displayGovernorCpus(const PlacementPlan& placement);

/// The governor reading for these inputs. Receivers that did no work
/// (idle) or that the display cannot relieve are left out, with their late
/// blocks and input waits. CPU is the share of displayGovernorCpus() while
/// placement is active (saturated housekeeping cores starve Opus and
/// sending however idle the other signal processing cores are), otherwise
/// all cores.
DisplayLoadReading displayLoadReadingFrom(const DisplayLoadInputs& inputs, qint64 nowMs,
                                          const DisplayBudgetCharge& acceptedCharge);

} // namespace NereusSDR
