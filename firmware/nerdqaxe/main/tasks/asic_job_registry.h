#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

// Portable runtime foundation, not enabled physical dispatch or hardware proof.
// A single owner or an external mutex must serialize EVERY registry operation.
// No elapsed time is accepted as evidence that a hardware pipeline was drained.
namespace FiveTratumJobs {

constexpr std::size_t kMaxWorkPayloadBytes = 96;

struct ConnectionToken {
    uint8_t asicIndex = 0xff;
    uint64_t generation = 0;
    uint64_t transportId = 0;
};

struct WorkToken {
    ConnectionToken connection;
    uint64_t generation = 0;
};

struct WorkPayload {
    std::array<uint8_t, kMaxWorkPayloadBytes> bytes{};
    std::size_t size = 0;
};

// Opaque route/job tokens identify retained immutable upstream job data. The
// caller must retain that data for the lease lifetime; this core owns no ESP,
// Stratum, heap, NVS, socket or UART objects. Payload bytes are copied here.
struct JobStamp {
    uint64_t routeToken = 0;
    uint64_t upstreamJobToken = 0;
    uint32_t versionBase = 0;
    uint32_t versionMask = 0;
    double poolDifficulty = 0;
    WorkPayload payload;
};

struct Lease {
    WorkToken owner;
    uint8_t wireId = 0xff;
    uint64_t serial = 0;
};

struct ContextSnapshot {
    Lease lease;
    JobStamp job;
};

struct DrainTicket {
    WorkToken owner;
    uint64_t serial = 0;
};

// fullVersion must be the actual reconstructed ASIC version. Do not mask away
// incompatible returned bits to manufacture a different candidate header.
inline bool isVersionCompatible(uint32_t base, uint32_t negotiatedMask,
                                uint32_t fullVersion) {
    return ((fullVersion ^ base) & ~negotiatedMask) == 0;
}

class AsicJobRegistry {
public:
    static constexpr uint16_t kMaxAsics = 4;
    static constexpr uint16_t kSlotsPerAsic = 16;
    static constexpr std::size_t kRememberedSubmissions = 16;

    enum class SubmissionRecord {
        Recorded,
        Duplicate,
        InvalidContext,
        IncompatibleVersion,
        CapacityExhausted,
    };

    explicit AsicJobRegistry(uint16_t asicCount = kMaxAsics)
        : m_asicCount(asicCount > 0 && asicCount <= kMaxAsics ? asicCount : 0) {}
    AsicJobRegistry(const AsicJobRegistry &) = delete;
    AsicJobRegistry &operator=(const AsicJobRegistry &) = delete;
    AsicJobRegistry(AsicJobRegistry &&) = delete;
    AsicJobRegistry &operator=(AsicJobRegistry &&) = delete;

    // BM1370's normalized reply namespace is 0,8,...,120, NOT 128 slots.
    // The driver owns raw reply decoding and hardware targeting verification.
    static bool isNormalizedWireId(uint16_t wireId) {
        return wireId <= 120 && (wireId & 7) == 0;
    }

    static bool validJob(const JobStamp &job) {
        return job.routeToken != 0 && job.upstreamJobToken != 0
            && std::isfinite(job.poolDifficulty) && job.poolDifficulty > 0
            && job.payload.size > 0 && job.payload.size <= kMaxWorkPayloadBytes;
    }

    bool beginSession(uint16_t asicIndex, uint64_t transportId,
                      ConnectionToken &out) {
        out = {};
        if (!validAsic(asicIndex) || transportId == 0) return false;
        AsicState &asic = m_asics[asicIndex];
        if (asic.faulted || !canAdvance(asic.connectionGeneration)
            || !canAdvance(asic.workGeneration)) return fault(asicIndex);
        retireUsed(asicIndex);
        ++asic.connectionGeneration;
        ++asic.workGeneration;
        asic.transportId = transportId;
        asic.active = true;
        asic.draining = false;
        asic.drainSerial = 0;
        out = connectionOf(asicIndex);
        return true;
    }

    bool currentWork(const ConnectionToken &connection, WorkToken &out) const {
        out = {};
        if (!matches(connection) || m_asics[connection.asicIndex].draining) return false;
        out = workOf(connection.asicIndex);
        return true;
    }

    // Reserve AND publish before dispatch. A Reserved slot cannot resolve a
    // nonce. Even cancellation quarantines its ID: a partial write is possible.
    bool reserve(const WorkToken &owner, uint16_t wireId, const JobStamp &job,
                 Lease &out) {
        out = {};
        if (!matches(owner) || !isNormalizedWireId(wireId) || !validJob(job)) return false;
        Slot &slot = m_slots[owner.connection.asicIndex][wireId / 8];
        if (slot.state != SlotState::Free) return false;
        uint64_t serial = 0;
        if (!nextSerial(serial)) return false;
        slot = {};
        slot.context.lease = {owner, static_cast<uint8_t>(wireId), serial};
        slot.context.job = job;
        slot.state = SlotState::Reserved;
        out = slot.context.lease;
        return true;
    }

    bool publish(const Lease &lease) {
        Slot *slot = matchingSlot(lease);
        if (!slot || slot->state != SlotState::Reserved) return false;
        slot->state = SlotState::Published;
        return true;
    }

    bool lookup(uint16_t asicIndex, uint16_t wireId, ContextSnapshot &out) const {
        out = {};
        if (!validAsic(asicIndex) || !isNormalizedWireId(wireId)) return false;
        const Slot &slot = m_slots[asicIndex][wireId / 8];
        if (slot.state != SlotState::Published || !matches(slot.context.lease.owner)) return false;
        out = slot.context;
        return true;
    }

    // Recheck a copied result/submission immediately before dispatch/submission.
    // A later clean, disconnect, drain or replacement invalidates the lease.
    bool isCurrent(const Lease &lease) const {
        const Slot *slot = matchingSlot(lease);
        return slot && slot->state == SlotState::Published;
    }

    bool cancel(const Lease &lease) {
        Slot *slot = matchingSlot(lease);
        if (!slot || (slot->state != SlotState::Reserved
            && slot->state != SlotState::Published)) return false;
        slot->state = SlotState::Quarantined;
        return true;
    }

    bool cleanWork(const ConnectionToken &connection, WorkToken &out) {
        out = {};
        if (!matches(connection)) return false;
        const uint16_t index = connection.asicIndex;
        AsicState &asic = m_asics[index];
        if (!canAdvance(asic.workGeneration)) return fault(index);
        retireUsed(index);
        ++asic.workGeneration;
        asic.draining = false;
        asic.drainSerial = 0;
        out = workOf(index);
        return true;
    }

    // Connection identity deliberately excludes the work generation: the
    // current socket can disconnect after a clean, but an old socket cannot
    // disconnect a replacement session merely because it owns the same chip.
    bool disconnect(const ConnectionToken &connection) {
        if (!matches(connection)) return false;
        const uint16_t index = connection.asicIndex;
        AsicState &asic = m_asics[index];
        retireUsed(index);
        if (!canAdvance(asic.connectionGeneration)
            || !canAdvance(asic.workGeneration)) return fault(index);
        ++asic.connectionGeneration;
        ++asic.workGeneration;
        asic.active = false;
        asic.transportId = 0;
        asic.draining = false;
        asic.drainSerial = 0;
        return true;
    }

    // Enter a dispatch-blocked epoch BEFORE the caller performs its verified
    // chip-stop/pipeline/UART drain procedure. This also retires copied results.
    bool beginDrain(const ConnectionToken &connection, DrainTicket &out) {
        out = {};
        if (!matches(connection)) return false;
        const uint16_t index = connection.asicIndex;
        AsicState &asic = m_asics[index];
        if (!canAdvance(asic.workGeneration)) return fault(index);
        uint64_t serial = 0;
        if (!nextSerial(serial)) return false;
        quarantineAll(index);
        ++asic.workGeneration;
        asic.draining = true;
        asic.drainSerial = serial;
        out = {workOf(index), serial};
        return true;
    }

    // This is an explicit CALLER ATTESTATION, not hardware verification by this
    // core. Never call because a timeout expired. A stale/replayed ticket fails.
    // A shared UART drain must coordinate every affected chip at the caller.
    bool confirmVerifiedDrain(const DrainTicket &ticket) {
        if (!matches(ticket.owner.connection)) return false;
        const uint16_t index = ticket.owner.connection.asicIndex;
        AsicState &asic = m_asics[index];
        if (!asic.draining || ticket.owner.generation != asic.workGeneration
            || ticket.serial == 0 || ticket.serial != asic.drainSerial) return false;
        for (Slot &slot : m_slots[index]) {
            slot = {};
            slot.state = SlotState::Free;
        }
        asic.draining = false;
        asic.drainSerial = 0;
        return true;
    }

    std::size_t availableSlots(uint16_t asicIndex) const {
        if (!validAsic(asicIndex) || !m_asics[asicIndex].active
            || m_asics[asicIndex].draining || m_asics[asicIndex].faulted) return 0;
        std::size_t free = 0;
        for (const Slot &slot : m_slots[asicIndex]) {
            if (slot.state == SlotState::Free) ++free;
        }
        return free;
    }

    // Call only AFTER hashing the exact immutable job and checking difficulty.
    // Recorded means remembered locally; it does not mean submitted/accepted.
    // No eviction: exhaustion rejects further records rather than forgetting a
    // duplicate. Context/chip scope is implicit in the validated lease.
    SubmissionRecord rememberSubmission(const Lease &lease, uint32_t nonce,
                                        uint32_t fullVersion) {
        Slot *slot = matchingSlot(lease);
        if (!slot || slot->state != SlotState::Published) return SubmissionRecord::InvalidContext;
        if (!isVersionCompatible(slot->context.job.versionBase,
                                 slot->context.job.versionMask, fullVersion)) {
            return SubmissionRecord::IncompatibleVersion;
        }
        const uint64_t key = (static_cast<uint64_t>(nonce) << 32) | fullVersion;
        for (std::size_t i = 0; i < slot->submissionCount; ++i) {
            if (slot->submissions[i] == key) return SubmissionRecord::Duplicate;
        }
        if (slot->submissionCount == kRememberedSubmissions) return SubmissionRecord::CapacityExhausted;
        slot->submissions[slot->submissionCount++] = key;
        return SubmissionRecord::Recorded;
    }

private:
    enum class SlotState { Free, Reserved, Published, Quarantined };
    struct Slot {
        // Startup is fail closed too: initial chip/UART state is unknown here.
        SlotState state = SlotState::Quarantined;
        ContextSnapshot context;
        std::array<uint64_t, kRememberedSubmissions> submissions{};
        std::size_t submissionCount = 0;
    };
    struct AsicState {
        uint64_t connectionGeneration = 0;
        uint64_t workGeneration = 0;
        uint64_t transportId = 0;
        uint64_t drainSerial = 0;
        bool active = false;
        bool draining = false;
        bool faulted = false;
    };

    uint16_t m_asicCount;
    uint64_t m_lastSerial = 0;
    std::array<AsicState, kMaxAsics> m_asics{};
    std::array<std::array<Slot, kSlotsPerAsic>, kMaxAsics> m_slots{};

    bool validAsic(uint16_t index) const { return index < m_asicCount; }
    static bool canAdvance(uint64_t value) { return value != std::numeric_limits<uint64_t>::max(); }
    bool nextSerial(uint64_t &out) {
        if (!canAdvance(m_lastSerial)) return false;
        out = ++m_lastSerial;
        return true;
    }
    ConnectionToken connectionOf(uint16_t index) const {
        const AsicState &asic = m_asics[index];
        return {static_cast<uint8_t>(index), asic.connectionGeneration, asic.transportId};
    }
    WorkToken workOf(uint16_t index) const { return {connectionOf(index), m_asics[index].workGeneration}; }
    bool matches(const ConnectionToken &token) const {
        if (!validAsic(token.asicIndex)) return false;
        const AsicState &asic = m_asics[token.asicIndex];
        return asic.active && !asic.faulted && token.generation != 0
            && token.generation == asic.connectionGeneration
            && token.transportId != 0 && token.transportId == asic.transportId;
    }
    bool matches(const WorkToken &token) const {
        return matches(token.connection) && !m_asics[token.connection.asicIndex].draining
            && token.generation == m_asics[token.connection.asicIndex].workGeneration;
    }
    const Slot *matchingSlot(const Lease &lease) const {
        if (!matches(lease.owner) || !isNormalizedWireId(lease.wireId) || lease.serial == 0) return nullptr;
        const Slot &slot = m_slots[lease.owner.connection.asicIndex][lease.wireId / 8];
        return slot.context.lease.serial == lease.serial ? &slot : nullptr;
    }
    Slot *matchingSlot(const Lease &lease) {
        return const_cast<Slot *>(static_cast<const AsicJobRegistry *>(this)->matchingSlot(lease));
    }
    void retireUsed(uint16_t index) {
        for (Slot &slot : m_slots[index]) {
            if (slot.state == SlotState::Reserved || slot.state == SlotState::Published) {
                slot.state = SlotState::Quarantined;
            }
        }
    }
    void quarantineAll(uint16_t index) {
        for (Slot &slot : m_slots[index]) slot.state = SlotState::Quarantined;
    }
    bool fault(uint16_t index) {
        if (validAsic(index)) {
            quarantineAll(index);
            m_asics[index].active = false;
            m_asics[index].faulted = true;
            m_asics[index].draining = false;
        }
        return false;
    }
};

} // namespace FiveTratumJobs
