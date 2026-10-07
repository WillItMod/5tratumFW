#pragma once

#include "asic_job_registry.h"

namespace FiveTratumJobs {

// A transport implements actual isolated work delivery. The existing BM1370
// UART driver does not implement this interface. A host mock is not hardware
// qualification and cannot enable this path in the running firmware.
class IsolatedWorkTransport {
public:
    virtual ~IsolatedWorkTransport() = default;
    virtual bool isolationVerified() const = 0;
    // Includes the qualified shared ticket/rolling-mask policy. Four separate
    // pool templates do not establish that four version-mask modes coexist.
    virtual bool acceptsJob(const JobStamp &job) const = 0;
    virtual bool sendWork(const ContextSnapshot &context) = 0;
};

enum class SelectionResult {
    Unsupported,
    Idle,
    Backpressure,
    Sent,
    IncompatibleJob,
    TransportFailed,
    RetiredDuringDispatch
};

// NerdQAxe++ policy: four independent latest-job mailboxes, one serialized
// scheduler/UART owner. Caller serializes this policy and its registry together.
// Pool credentials and clock/voltage settings do not enter this object.
class AsicJobSelector {
public:
    static constexpr unsigned AsicCount = 4;

    explicit AsicJobSelector(AsicJobRegistry &registry) : registry_(registry) {}

    bool stage(const WorkToken &owner, const JobStamp &job) {
        const unsigned chip = owner.connection.asicIndex;
        if (chip >= AsicCount || !current(owner)
                || !AsicJobRegistry::validJob(job)) return false;
        pending_[chip] = Pending{true, owner, job};
        return true;
    }

    SelectionResult dispatchNext(IsolatedWorkTransport &transport) {
        if (!transport.isolationVerified()) return SelectionResult::Unsupported;
        bool blocked = false;
        for (unsigned offset = 0; offset < AsicCount; ++offset) {
            const unsigned chip = (nextChip_ + offset) % AsicCount;
            Pending &pending = pending_[chip];
            if (!pending.available) continue;
            if (!current(pending.owner)) {
                pending.available = false;
                continue;
            }
            if (!transport.acceptsJob(pending.job)) {
                pending.available = false;
                nextChip_ = (chip + 1) % AsicCount;
                return SelectionResult::IncompatibleJob;
            }
            Lease lease{};
            bool reserved = false;
            for (unsigned offsetSlot = 0; offsetSlot < 16; ++offsetSlot) {
                const unsigned slot = (nextSlot_[chip] + offsetSlot) % 16;
                // BM1370's normalized contexts are 00,08,...,78. Do not use
                // ASIC addresses as job IDs or add an address to a work frame.
                if (registry_.reserve(pending.owner, slot * 8, pending.job, lease)) {
                    nextSlot_[chip] = (slot + 1) % 16;
                    reserved = true;
                    break;
                }
            }
            if (!reserved) {
                blocked = true;
                continue;
            }
            if (!registry_.publish(lease)) {
                registry_.cancel(lease);
                pending.available = false;
                continue;
            }
            ContextSnapshot context{};
            if (!registry_.lookup(chip, lease.wireId, context)
                    || context.lease.serial != lease.serial) {
                registry_.cancel(lease);
                pending.available = false;
                continue;
            }
            // Publish before invoking the transport: even an immediate result
            // must have its immutable original route/header context available.
            pending.available = false;
            nextChip_ = (chip + 1) % AsicCount;
            if (!registry_.isCurrent(lease)) return SelectionResult::RetiredDuringDispatch;
            if (!transport.sendWork(context)) {
                registry_.cancel(lease);
                return SelectionResult::TransportFailed;
            }
            return registry_.isCurrent(lease) ? SelectionResult::Sent
                                             : SelectionResult::RetiredDuringDispatch;
        }
        return blocked ? SelectionResult::Backpressure : SelectionResult::Idle;
    }

private:
    struct Pending {
        bool available = false;
        WorkToken owner{};
        JobStamp job{};
    };

    bool current(const WorkToken &owner) const {
        WorkToken actual{};
        return registry_.currentWork(owner.connection, actual)
            && actual.generation == owner.generation;
    }

    AsicJobRegistry &registry_;
    Pending pending_[AsicCount]{};
    unsigned nextSlot_[AsicCount]{};
    unsigned nextChip_ = 0;
};

} // namespace FiveTratumJobs
