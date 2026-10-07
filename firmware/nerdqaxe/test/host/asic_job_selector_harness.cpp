#include "asic_job_selector.h"

#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

using namespace FiveTratumJobs;

struct Fixture {
    AsicJobRegistry registry;
    AsicJobSelector selector{registry};
    ConnectionToken connections[4]{};
    WorkToken owners[4]{};

    explicit Fixture(bool simulateDrained = true) {
        for (unsigned chip = 0; chip < 4; ++chip) {
            assert(registry.beginSession(chip, 100 + chip, connections[chip]));
            if (simulateDrained) {
                DrainTicket ticket{};
                assert(registry.beginDrain(connections[chip], ticket));
                // Mock attestation only; this harness has no hardware/UART I/O.
                assert(registry.confirmVerifiedDrain(ticket));
            }
            assert(registry.currentWork(connections[chip], owners[chip]));
        }
    }
    static JobStamp job(unsigned chip) {
        JobStamp job{};
        job.routeToken = 1000 + chip;
        job.upstreamJobToken = 2000 + chip;
        job.versionBase = 0x20000000;
        job.versionMask = 0x1fffe000;
        job.poolDifficulty = chip == 0 ? 0.5 : 128 * chip;
        job.payload.size = 80;
        job.payload.bytes.fill(static_cast<uint8_t>(chip + 1));
        return job;
    }
    void stageAll() {
        for (unsigned chip = 0; chip < 4; ++chip) assert(selector.stage(owners[chip], job(chip)));
    }
};

struct MockTransport : IsolatedWorkTransport {
    bool verified = true;
    bool succeed = true;
    std::vector<ContextSnapshot> sent;
    std::function<void(const ContextSnapshot &)> duringSend;
    bool isolationVerified() const override { return verified; }
    bool acceptsJob(const JobStamp &job) const override { return job.versionMask == 0x1fffe000; }
    bool sendWork(const ContextSnapshot &context) override {
        sent.push_back(context);
        if (duringSend) duringSend(context);
        return succeed;
    }
};

void fourIndependentJobs() {
    Fixture f;
    MockTransport transport;
    f.stageAll();
    for (unsigned chip = 0; chip < 4; ++chip) {
        assert(f.selector.dispatchNext(transport) == SelectionResult::Sent);
        const ContextSnapshot &context = transport.sent.back();
        assert(context.lease.owner.connection.asicIndex == chip);
        assert(context.job.routeToken == 1000 + chip);
        assert(context.job.upstreamJobToken == 2000 + chip);
        assert(context.job.payload.bytes[0] == chip + 1);
        assert(context.job.poolDifficulty == (chip == 0 ? 0.5 : 128 * chip));
        assert(context.lease.wireId == 0); // same ID, separate chip namespaces
        assert(f.registry.isCurrent(context.lease));
    }
    assert(f.selector.dispatchNext(transport) == SelectionResult::Idle);
    assert(transport.sent.size() == 4);
    std::cout << "{\"source\":\"simulation\",\"asicJobs\":4,\"independentWorkAssignment\":false}" << std::endl;
}

void unsupportedDoesNotDispatch() {
    Fixture f;
    MockTransport transport;
    transport.verified = false;
    f.stageAll();
    assert(f.selector.dispatchNext(transport) == SelectionResult::Unsupported);
    assert(transport.sent.empty());
    for (unsigned chip = 0; chip < 4; ++chip) assert(f.registry.availableSlots(chip) == 16);
    transport.verified = true;
    assert(f.selector.dispatchNext(transport) == SelectionResult::Sent);
}

void startupAndExhaustion() {
    Fixture startup(false);
    MockTransport transport;
    startup.stageAll();
    assert(startup.selector.dispatchNext(transport) == SelectionResult::Backpressure);
    assert(transport.sent.empty());
    Fixture f;
    for (unsigned i = 0; i < 16; ++i) {
        assert(f.selector.stage(f.owners[0], Fixture::job(0)));
        assert(f.selector.dispatchNext(transport) == SelectionResult::Sent);
        assert(transport.sent.back().lease.wireId == i * 8);
    }
    assert(f.selector.stage(f.owners[0], Fixture::job(0)));
    assert(f.selector.stage(f.owners[1], Fixture::job(1)));
    assert(f.selector.dispatchNext(transport) == SelectionResult::Sent);
    assert(transport.sent.back().lease.owner.connection.asicIndex == 1);
    assert(f.selector.dispatchNext(transport) == SelectionResult::Backpressure);
    assert(f.registry.availableSlots(0) == 0);
}

void cleanAndReconnectBeforeSend() {
    Fixture f;
    MockTransport transport;
    f.stageAll();
    WorkToken cleaned{};
    assert(f.registry.cleanWork(f.connections[0], cleaned));
    ConnectionToken reconnected{};
    assert(f.registry.beginSession(1, 999, reconnected));
    assert(f.selector.dispatchNext(transport) == SelectionResult::Sent);
    assert(transport.sent.back().lease.owner.connection.asicIndex == 2);
    assert(!f.selector.stage(f.owners[0], Fixture::job(0)));
    assert(!f.selector.stage(f.owners[1], Fixture::job(1)));
}

void cleanDuringSend() {
    Fixture f;
    MockTransport transport;
    transport.duringSend = [&](const ContextSnapshot &context) {
        // Even a synchronous receive can resolve the already published job.
        ContextSnapshot immediate{};
        assert(f.registry.lookup(0, context.lease.wireId, immediate));
        assert(immediate.lease.serial == context.lease.serial);
        WorkToken cleaned{};
        assert(f.registry.cleanWork(f.connections[0], cleaned));
    };
    assert(f.selector.stage(f.owners[0], Fixture::job(0)));
    assert(f.selector.dispatchNext(transport) == SelectionResult::RetiredDuringDispatch);
    assert(!f.registry.isCurrent(transport.sent.back().lease));
}

void transportFailure() {
    Fixture f;
    MockTransport transport;
    transport.succeed = false;
    assert(f.selector.stage(f.owners[0], Fixture::job(0)));
    assert(f.selector.dispatchNext(transport) == SelectionResult::TransportFailed);
    assert(!f.registry.isCurrent(transport.sent.back().lease));
    assert(f.registry.availableSlots(0) == 15);
    transport.succeed = true;
    assert(f.selector.stage(f.owners[0], Fixture::job(0)));
    assert(f.selector.dispatchNext(transport) == SelectionResult::Sent);
    assert(transport.sent.back().lease.wireId == 8);
}

void immutableLatestJob() {
    Fixture f;
    MockTransport transport;
    JobStamp first = Fixture::job(0);
    assert(f.selector.stage(f.owners[0], first));
    first.routeToken = 3333;
    first.payload.bytes[0] = 77;
    assert(f.selector.stage(f.owners[0], first));
    first.routeToken = 9999;
    first.payload.bytes[0] = 88;
    assert(f.selector.dispatchNext(transport) == SelectionResult::Sent);
    assert(transport.sent.back().job.routeToken == 3333);
    assert(transport.sent.back().job.payload.bytes[0] == 77);
}

void incompatibleAndInvalidJob() {
    Fixture f;
    MockTransport transport;
    JobStamp invalid = Fixture::job(0);
    invalid.routeToken = 0;
    assert(!f.selector.stage(f.owners[0], invalid));
    JobStamp incompatible = Fixture::job(0);
    incompatible.versionMask = 0x1f00e000;
    assert(f.selector.stage(f.owners[0], incompatible));
    assert(f.selector.dispatchNext(transport) == SelectionResult::IncompatibleJob);
    assert(transport.sent.empty());
    assert(f.registry.availableSlots(0) == 16);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string scenario = argv[1];
    if (scenario == "four-jobs") fourIndependentJobs();
    else if (scenario == "unsupported") unsupportedDoesNotDispatch();
    else if (scenario == "exhaustion") startupAndExhaustion();
    else if (scenario == "clean-before") cleanAndReconnectBeforeSend();
    else if (scenario == "clean-during") cleanDuringSend();
    else if (scenario == "transport-failure") transportFailure();
    else if (scenario == "immutable") immutableLatestJob();
    else if (scenario == "incompatible") incompatibleAndInvalidJob();
    else return 2;
}
