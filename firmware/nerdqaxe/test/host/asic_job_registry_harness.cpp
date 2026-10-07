#include "tasks/asic_job_registry.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

using namespace FiveTratumJobs;

static_assert(!std::is_copy_constructible<AsicJobRegistry>::value, "live registry cannot duplicate token ownership");
static_assert(!std::is_move_constructible<AsicJobRegistry>::value, "live registry cannot move away from its serialized owner");

static void require(bool condition, const char *reason) {
    if (!condition) throw std::runtime_error(reason);
}

static JobStamp job(uint64_t route = 1, uint64_t upstream = 1) {
    JobStamp result;
    result.routeToken = route;
    result.upstreamJobToken = upstream;
    result.versionBase = 0x20000000;
    result.versionMask = 0x1fffe000;
    result.poolDifficulty = 512;
    result.payload.size = 4;
    result.payload.bytes = {0x11, 0x22, 0x33, 0x44};
    return result;
}

static ConnectionToken ready(AsicJobRegistry &registry, uint16_t chip = 0,
                             uint64_t transport = 100) {
    ConnectionToken connection;
    require(registry.beginSession(chip, transport, connection), "session starts");
    DrainTicket drain;
    require(registry.beginDrain(connection, drain), "drain epoch starts");
    // Simulated external verified drain only. No UART/hardware proof occurs.
    require(registry.confirmVerifiedDrain(drain), "mock verified drain completes");
    return connection;
}

static Lease published(AsicJobRegistry &registry, const ConnectionToken &connection,
                       uint16_t wire = 0, const JobStamp &stamp = job()) {
    WorkToken work;
    Lease lease;
    require(registry.currentWork(connection, work), "current work available");
    require(registry.reserve(work, wire, stamp, lease), "context reserves");
    require(registry.publish(lease), "context publishes");
    return lease;
}

static void isolation() {
    AsicJobRegistry registry;
    Lease leases[4];
    for (uint16_t chip = 0; chip < 4; ++chip) {
        const ConnectionToken connection = ready(registry, chip, 100 + chip);
        JobStamp stamp = job(chip + 1, 1000 + chip);
        stamp.payload.bytes[0] = static_cast<uint8_t>(chip);
        leases[chip] = published(registry, connection, 24, stamp);
    }
    for (uint16_t chip = 0; chip < 4; ++chip) {
        ContextSnapshot snapshot;
        require(registry.lookup(chip, 24, snapshot), "same wire ID resolves per chip");
        require(snapshot.lease.serial == leases[chip].serial, "exact chip lease returned");
        require(snapshot.lease.owner.connection.transportId == 100 + chip, "original socket retained");
        require(snapshot.job.routeToken == chip + 1, "original route retained");
        require(snapshot.job.upstreamJobToken == 1000 + chip, "original upstream job retained");
        require(snapshot.job.payload.bytes[0] == chip, "original chip payload retained");
    }
}

static void immutablePayload() {
    AsicJobRegistry registry;
    const ConnectionToken connection = ready(registry);
    JobStamp original = job(7, 19);
    original.poolDifficulty = 0.5;
    const Lease lease = published(registry, connection, 0, original);
    original.routeToken = 999;
    original.upstreamJobToken = 888;
    original.poolDifficulty = 1;
    original.payload.bytes.fill(0xff);
    ContextSnapshot first;
    require(registry.lookup(0, 0, first), "published snapshot exists");
    require(first.job.routeToken == 7 && first.job.upstreamJobToken == 19, "route input copied");
    require(first.job.poolDifficulty == 0.5, "fractional difficulty preserved");
    require(first.job.payload.bytes[0] == 0x11 && first.job.payload.size == 4, "payload input copied");
    first.job.routeToken = 777;
    first.job.payload.bytes[0] = 0xee;
    ContextSnapshot second;
    require(registry.lookup(0, 0, second), "second snapshot exists");
    require(second.job.routeToken == 7 && second.job.payload.bytes[0] == 0x11, "snapshot cannot mutate registry");
    require(registry.isCurrent(lease), "original lease still current");
    Lease fabricated = lease;
    ++fabricated.serial;
    require(!registry.isCurrent(fabricated), "wrong reservation identity rejected");
}

static void publishBeforeDispatch() {
    AsicJobRegistry registry;
    const ConnectionToken connection = ready(registry);
    WorkToken work;
    require(registry.currentWork(connection, work), "work exists");
    Lease lease;
    require(registry.reserve(work, 0, job(), lease), "reserve succeeds");
    ContextSnapshot before;
    require(!registry.lookup(0, 0, before) && !registry.isCurrent(lease), "unpublished result rejected");
    require(registry.publish(lease), "publish succeeds before dispatch callback");
    ContextSnapshot immediateResult;
    require(registry.lookup(0, 0, immediateResult), "immediate callback has immutable context");
    require(immediateResult.lease.serial == lease.serial, "immediate result matches reservation");
    require(!registry.publish(lease), "cannot publish twice");
    Lease other;
    require(!registry.reserve(work, 0, job(2), other), "occupied wire slot cannot be overwritten");
}

static void startupBarrier() {
    AsicJobRegistry registry;
    ConnectionToken connection;
    require(registry.beginSession(0, 10, connection), "initial session starts");
    WorkToken work;
    require(registry.currentWork(connection, work), "initial work token exists");
    Lease lease;
    require(registry.availableSlots(0) == 0, "startup pipeline state unknown");
    require(!registry.reserve(work, 0, job(), lease), "startup reservation blocked");
    require(!registry.confirmVerifiedDrain(DrainTicket{}), "empty drain proof rejected");
    DrainTicket drain;
    require(registry.beginDrain(connection, drain), "explicit drain starts");
    require(!registry.currentWork(connection, work), "dispatch blocked while draining");
    require(registry.confirmVerifiedDrain(drain), "explicit simulated verified barrier opens slots");
    require(registry.availableSlots(0) == 16, "all sixteen normalized slots become available");
    require(!registry.confirmVerifiedDrain(drain), "drain ticket cannot be replayed");
}

static void reconnect() {
    AsicJobRegistry registry;
    const ConnectionToken oldConnection = ready(registry);
    const Lease oldLease = published(registry, oldConnection);
    ContextSnapshot copiedResult;
    require(registry.lookup(0, 0, copiedResult), "old result copied before disconnect");
    require(registry.disconnect(oldConnection), "current session disconnects");
    require(!registry.isCurrent(copiedResult.lease), "copied result cannot submit after disconnect");
    ConnectionToken replacement;
    require(registry.beginSession(0, 200, replacement), "replacement session starts");
    require(!registry.disconnect(oldConnection), "old socket callback cannot close replacement");
    WorkToken fresh;
    require(registry.currentWork(replacement, fresh), "replacement stays active");
    Lease lease;
    require(!registry.reserve(fresh, 0, job(2), lease), "old wire ID remains quarantined across reconnect");
    require(registry.reserve(fresh, 8, job(2), lease), "never-used wire ID remains usable");
    require(registry.publish(lease), "fresh context publishes");
    require(!registry.isCurrent(oldLease), "old generation remains invalid");
}

static void cleanInflightRace() {
    AsicJobRegistry registry;
    const ConnectionToken connection = ready(registry);
    WorkToken builtFrom;
    require(registry.currentWork(connection, builtFrom), "template snapshot token exists");
    const Lease sent = published(registry, connection, 0);
    Lease reserved;
    require(registry.reserve(builtFrom, 8, job(), reserved), "second job built and reserved");
    ContextSnapshot cloned;
    require(registry.lookup(0, 0, cloned), "result cloned before clean");
    WorkToken next;
    require(registry.cleanWork(connection, next), "clean notification advances epoch");
    require(!registry.publish(reserved), "built reservation cannot resurrect after clean");
    Lease late;
    require(!registry.reserve(builtFrom, 16, job(), late), "late build token cannot reinsert old work");
    require(!registry.isCurrent(sent) && !registry.isCurrent(cloned.lease), "in-flight copied contexts invalidated");
    require(!registry.reserve(next, 0, job(), late) && !registry.reserve(next, 8, job(), late), "retired IDs quarantined");
    require(registry.reserve(next, 16, job(3), late) && registry.publish(late), "fresh work on unused ID succeeds");
    require(registry.disconnect(connection), "same socket can disconnect after clean generation changed");
    require(!registry.isCurrent(late), "disconnect retires new clean epoch too");
}

static void chipLocalClean() {
    AsicJobRegistry registry;
    const ConnectionToken first = ready(registry, 0, 10);
    const ConnectionToken second = ready(registry, 1, 11);
    const Lease firstLease = published(registry, first, 0, job(1));
    const Lease secondLease = published(registry, second, 0, job(2));
    WorkToken next;
    require(registry.cleanWork(first, next), "chip zero cleaned");
    require(!registry.isCurrent(firstLease), "chip zero job retired");
    require(registry.isCurrent(secondLease), "chip one independent route retained");
    require(registry.disconnect(first), "chip zero disconnects");
    require(registry.isCurrent(secondLease), "chip one unaffected by other reconnect");
}

static void exhaustionAndReuse() {
    AsicJobRegistry registry;
    const ConnectionToken connection = ready(registry);
    Lease original[16];
    for (uint16_t slot = 0; slot < 16; ++slot) original[slot] = published(registry, connection, slot * 8, job(1, slot + 1));
    require(registry.availableSlots(0) == 0, "sixteen slots exhaust without collision");
    WorkToken work;
    require(registry.currentWork(connection, work), "work still exists");
    Lease replacement;
    require(!registry.reserve(work, 0, job(2), replacement), "seventeenth dispatch cannot overwrite live job");
    for (const Lease &lease : original) require(registry.cancel(lease), "each prior context retires");
    require(registry.availableSlots(0) == 0, "cancellation never frees hardware IDs");
    require(!registry.reserve(work, 0, job(2), replacement), "retirement cannot be treated as hardware drain");
    DrainTicket drain;
    require(registry.beginDrain(connection, drain), "explicit verified reuse barrier starts");
    require(registry.confirmVerifiedDrain(drain), "mock verified drain releases quarantine");
    require(registry.currentWork(connection, work), "new drain epoch exists");
    require(registry.reserve(work, 0, job(2), replacement) && registry.publish(replacement), "wire ID reused after barrier");
    require(replacement.serial != original[0].serial, "reservation identity never recycled");
    for (const Lease &lease : original) require(!registry.isCurrent(lease), "all old copies stay invalid after reuse");
}

static void drainEpochRace() {
    AsicJobRegistry registry;
    const ConnectionToken connection = ready(registry);
    const Lease old = published(registry, connection);
    DrainTicket first;
    require(registry.beginDrain(connection, first), "first drain starts");
    require(!registry.isCurrent(old), "entering drain retires old copied result");
    WorkToken work;
    require(!registry.currentWork(connection, work), "no dispatch token during drain");
    require(registry.cleanWork(connection, work), "intervening clean changes drain epoch");
    require(!registry.confirmVerifiedDrain(first), "old drain cannot certify changed work epoch");
    Lease lease;
    require(!registry.reserve(work, 0, job(), lease), "cancelled drain leaves IDs quarantined");
    DrainTicket second;
    require(registry.beginDrain(connection, second), "replacement drain starts");
    ConnectionToken replacement;
    require(registry.beginSession(0, 200, replacement), "socket replaced during drain");
    require(!registry.confirmVerifiedDrain(second), "old socket drain cannot open replacement IDs");
    require(!registry.disconnect(connection), "stale disconnect rejected after drain race");
    DrainTicket final;
    require(registry.beginDrain(replacement, final), "replacement socket requests fresh barrier");
    require(registry.confirmVerifiedDrain(final), "only replacement barrier unlocks reuse");
}

static void duplicates() {
    AsicJobRegistry registry;
    const ConnectionToken zero = ready(registry, 0, 100);
    const ConnectionToken one = ready(registry, 1, 101);
    const Lease first = published(registry, zero, 0, job(1, 1));
    const Lease second = published(registry, zero, 8, job(1, 2));
    const Lease otherChip = published(registry, one, 0, job(2, 1));
    using Result = AsicJobRegistry::SubmissionRecord;
    require(registry.rememberSubmission(first, 42, 0x20000000) == Result::Recorded, "first candidate recorded");
    require(registry.rememberSubmission(first, 42, 0x20000000) == Result::Duplicate, "same context candidate rejected twice");
    require(registry.rememberSubmission(second, 42, 0x20000000) == Result::Recorded, "same nonce on another job is distinct");
    require(registry.rememberSubmission(otherChip, 42, 0x20000000) == Result::Recorded, "same nonce on another chip is distinct");
    require(registry.rememberSubmission(first, 42, 0x20002000) == Result::Recorded, "rolled version distinguishes candidate");
    require(registry.cancel(first), "context retires");
    require(registry.rememberSubmission(first, 99, 0x20000000) == Result::InvalidContext, "retired candidates cannot submit");
}

static void boundedDuplicates() {
    AsicJobRegistry registry;
    const Lease lease = published(registry, ready(registry));
    using Result = AsicJobRegistry::SubmissionRecord;
    for (std::size_t n = 0; n < AsicJobRegistry::kRememberedSubmissions; ++n) {
        require(registry.rememberSubmission(lease, static_cast<uint32_t>(n), 0x20000000) == Result::Recorded, "bounded candidate recorded");
    }
    require(registry.rememberSubmission(lease, 9999, 0x20000000) == Result::CapacityExhausted, "capacity fails closed without evicting prior nonce");
    require(registry.rememberSubmission(lease, 0, 0x20000000) == Result::Duplicate, "oldest duplicate still remembered");
}

static void versionMask() {
    const uint32_t base = 0x20002000;
    require(isVersionCompatible(base, 0x0000e000, 0x20000000), "rolling may clear an allowed base bit");
    require(isVersionCompatible(base, 0x0000e000, 0x2000a000), "rolling may set allowed bits");
    require(!isVersionCompatible(base, 0x0000e000, 0x20010000), "outside-mask change rejected");
    require(!isVersionCompatible(base, 0, 0x20000000), "zero mask preserves exact base");
    AsicJobRegistry registry;
    JobStamp stamp = job();
    stamp.versionBase = base;
    stamp.versionMask = 0x0000e000;
    const Lease lease = published(registry, ready(registry), 0, stamp);
    using Result = AsicJobRegistry::SubmissionRecord;
    require(registry.rememberSubmission(lease, 1, 0x20010000) == Result::IncompatibleVersion, "record validates immutable negotiated mask");
    require(registry.rememberSubmission(lease, 1, 0x20000000) == Result::Recorded, "invalid version did not consume duplicate record");
}

static void invalidInputs() {
    AsicJobRegistry disabled(0), excessive(5), registry;
    ConnectionToken connection;
    require(!disabled.beginSession(0, 1, connection), "zero inventory disabled");
    require(!excessive.beginSession(0, 1, connection), "unsupported inventory disabled");
    require(!registry.beginSession(4, 1, connection), "out-of-range chip rejected");
    require(!registry.beginSession(65535, 1, connection), "wide chip index cannot truncate");
    require(!registry.beginSession(0, 0, connection), "missing transport identity rejected");
    connection = ready(registry);
    WorkToken work;
    require(registry.currentWork(connection, work), "valid token exists");
    for (uint16_t invalid : {1, 7, 121, 128, 256, 65535}) {
        Lease lease;
        ContextSnapshot result;
        require(!registry.reserve(work, invalid, job(), lease), "non-normalized wire ID rejected without truncation");
        require(!registry.lookup(0, invalid, result), "non-normalized result rejected");
    }
    for (double diff : {0.0, -0.5, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        JobStamp stamp = job();
        stamp.poolDifficulty = diff;
        require(!AsicJobRegistry::validJob(stamp), "invalid difficulty rejected before staging");
        Lease lease;
        require(!registry.reserve(work, 0, stamp, lease), "invalid difficulty cannot reserve");
    }
    for (std::size_t size : {0u, 97u, 255u, 256u, 257u, 65535u}) {
        JobStamp stamp = job();
        stamp.payload.size = size;
        require(!AsicJobRegistry::validJob(stamp), "invalid payload size rejected");
    }
    JobStamp stamp = job();
    stamp.routeToken = 0;
    require(!AsicJobRegistry::validJob(stamp), "missing route token rejected");
    stamp = job();
    stamp.upstreamJobToken = 0;
    require(!AsicJobRegistry::validJob(stamp), "missing upstream job identity rejected");
    require(registry.availableSlots(0) == 16, "rejected inputs consumed no slots");
    require(sizeof(AsicJobRegistry) <= 32768, "fixed registry memory remains bounded");
}

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    try {
        const char *scenario = argv[1];
        if (strcmp(scenario, "isolation") == 0) isolation();
        else if (strcmp(scenario, "immutable-payload") == 0) immutablePayload();
        else if (strcmp(scenario, "publish-before-dispatch") == 0) publishBeforeDispatch();
        else if (strcmp(scenario, "startup-barrier") == 0) startupBarrier();
        else if (strcmp(scenario, "reconnect") == 0) reconnect();
        else if (strcmp(scenario, "clean-inflight-race") == 0) cleanInflightRace();
        else if (strcmp(scenario, "chip-local-clean") == 0) chipLocalClean();
        else if (strcmp(scenario, "exhaustion-and-reuse") == 0) exhaustionAndReuse();
        else if (strcmp(scenario, "drain-epoch-race") == 0) drainEpochRace();
        else if (strcmp(scenario, "duplicates") == 0) duplicates();
        else if (strcmp(scenario, "bounded-duplicates") == 0) boundedDuplicates();
        else if (strcmp(scenario, "version-mask") == 0) versionMask();
        else if (strcmp(scenario, "invalid-inputs") == 0) invalidInputs();
        else return 3;
        std::cout << "{\"passed\":true,\"registryBytes\":" << sizeof(AsicJobRegistry)
                  << ",\"physicalDispatchEnabled\":false}";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
