#include "stratum/stratum_api.h"
#include "stratum/native_pool_stream.h"
#include "boards/five_tratum_model_labels.h"
#include "http_server/handler_capabilities.h"
#include "esp_ota_ops.h"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {
esp_app_desc_t description{};
bool identityAvailable = true;
std::string identity = "5tfw:0102030405060708090a0b0c0d0e0f10";
unsigned identityReads = 0;
}

const esp_app_desc_t *esp_app_get_description() { return &description; }
bool readFiveTratumPhysicalDeviceId(char output[FiveTratumCapabilities::DEVICE_ID_SIZE]) {
    ++identityReads;
    if (!identityAvailable) return false;
    std::snprintf(output, FiveTratumCapabilities::DEVICE_ID_SIZE, "%s", identity.c_str());
    return true;
}
void *heap_caps_malloc(std::size_t size, unsigned) { return std::malloc(size); }
void *heap_caps_calloc(std::size_t count, std::size_t size, unsigned) { return std::calloc(count, size); }
void *heap_caps_realloc(void *pointer, std::size_t size, unsigned) { return std::realloc(pointer, size); }
void heap_caps_free(void *pointer) { std::free(pointer); }

StratumTransport::StratumTransport(bool tls) : m_use_tls(tls), m_t(nullptr) {
    pthread_mutex_init(&m_lock, nullptr);
}
StratumTransport::~StratumTransport() { pthread_mutex_destroy(&m_lock); }
bool StratumTransport::connect(const char *, const char *, uint16_t) { return false; }
int StratumTransport::send(const void *, std::size_t) { return -1; }
int StratumTransport::recv(void *, std::size_t) { return -1; }
bool StratumTransport::isConnected() { return false; }
void StratumTransport::close() {}

class MockTransport final : public StratumTransport {
public:
    std::string bytes;
    std::size_t chunk = 7; // Exercise the actual send() partial-write loop.
    bool connected = true;
    bool fail = false;
    MockTransport() : StratumTransport(false) {}
    bool isConnected() override { return connected; }
    int send(const void *data, std::size_t length) override {
        if (fail) { errno = EIO; return -1; }
        const std::size_t count = std::min(length, chunk);
        bytes.append(static_cast<const char *>(data), count);
        return static_cast<int>(count);
    }
};

int main(int argc, char **argv) {
    assert(argc == 2);
    std::strcpy(description.version, "5tratumFW-qa-web-a10");
    const std::string scenario = argv[1];
    const bool octaxe = scenario.rfind("oct-", 0) == 0;
    if (octaxe) std::strcpy(description.version, "5tratumFW-oct-0.1.0-beta.1");
    if (scenario == "bounds") {
        char guarded[FiveTratumNativePool::AgentBufferBytes + 2];
        std::memset(guarded, 'Z', sizeof(guarded));
        char *agent = guarded + 1;
        assert(!FiveTratumNativePool::buildAgent(agent, 1, "QAxe", "BM1370", "a10", identity.c_str(), 0));
        assert(!agent[0] && guarded[0] == 'Z' && guarded[2] == 'Z');
        assert(!FiveTratumNativePool::buildAgent(nullptr, 0, "QAxe", "BM1370", "a10", identity.c_str(), 0));
        for (const char *bad : {"", "bad\"name", "bad\\name", "bad/name", "bad:name", "bad\nname", "bad\x7f"}) {
            assert(!FiveTratumNativePool::buildAgent(agent, FiveTratumNativePool::AgentBufferBytes,
                bad, "BM1370", "a10", identity.c_str(), 0));
            assert(!agent[0]);
        }
        const std::string longName(65, 'A');
        assert(!FiveTratumNativePool::buildAgent(agent, FiveTratumNativePool::AgentBufferBytes,
            longName.c_str(), "BM1370", "a10", identity.c_str(), 0));
        const std::string sixtyFour(64, 'A');
        assert(!FiveTratumNativePool::buildAgent(agent, FiveTratumNativePool::AgentBufferBytes,
            sixtyFour.c_str(), sixtyFour.c_str(), sixtyFour.c_str(), identity.c_str(), 0));
        const std::string thirtyTwo(32, 'B');
        const std::string eighteen(18, 'C');
        assert(FiveTratumNativePool::buildAgent(agent, FiveTratumNativePool::AgentBufferBytes,
            sixtyFour.c_str(), thirtyTwo.c_str(), eighteen.c_str(), identity.c_str(), 0));
        assert(std::strlen(agent) == FiveTratumNativePool::MaxAgentBytes);
        const std::string nineteen(19, 'C');
        assert(!FiveTratumNativePool::buildAgent(agent, FiveTratumNativePool::AgentBufferBytes,
            sixtyFour.c_str(), thirtyTwo.c_str(), nineteen.c_str(), identity.c_str(), 0));
        for (unsigned byte = 1; byte < 256; ++byte) {
            char component[] = {'A', static_cast<char>(byte), 'B', '\0'};
            const bool expected = byte >= 0x20 && byte <= 0x7e &&
                byte != '"' && byte != '\\' && byte != '/' && byte != ':';
            assert(FiveTratumNativePool::buildAgent(agent, FiveTratumNativePool::AgentBufferBytes,
                component, "BM1370", "a10", identity.c_str(), 0) == expected);
        }
        assert(guarded[0] == 'Z' && guarded[sizeof(guarded) - 1] == 'Z');
        std::cout << "{\"bounded\":true,\"unsafeComponentsRejected\":true}";
        return 0;
    }
    if (scenario == "two-streams" || scenario == "oct-two-streams") {
        StratumApi primary, secondary;
        MockTransport a, b;
        const char *device = octaxe ? FiveTratumModels::OctaxeGammaMiningAgent : "NerdQAxe++";
        assert(primary.subscribe(&a, device, "BM1370", 0));
        assert(secondary.subscribe(&b, device, "BM1370", 1));
        assert(identityReads == 2);
        std::cout << '[' << a.bytes.substr(0, a.bytes.size() - 1) << ','
                  << b.bytes.substr(0, b.bytes.size() - 1) << ']';
        return 0;
    }
    StratumApi api;
    MockTransport transport;
    int index = 0;
    const char *device = octaxe ? FiveTratumModels::OctaxeGammaMiningAgent : "NerdQAxe++";
    if (scenario == "legacy") index = -1;
    else if (scenario == "invalid-index") index = 2;
    else if (scenario == "unavailable-id") identityAvailable = false;
    else if (scenario == "zero-id") identity = "5tfw:00000000000000000000000000000000";
    else if (scenario == "uppercase-id") identity = "5tfw:0102030405060708090A0B0C0D0E0F10";
    else if (scenario == "short-id") identity = "5tfw:123";
    else if (scenario == "unsafe") device = "QAxe\"\n";
    else if (scenario == "disconnected") transport.connected = false;
    else if (scenario == "send-failure") transport.fail = true;
    else if (scenario == "oct-unavailable-id") identityAvailable = false;
    else if (scenario == "oct-utf8-model") device = FiveTratumModels::OctaxeGamma;
    const bool sent = scenario == "legacy" ? api.subscribe(&transport, device, "BM1370")
                                           : api.subscribe(&transport, device, "BM1370", index);
    if (scenario == "legacy" || scenario == "invalid-index") assert(identityReads == 0);
    if (scenario == "oct-utf8-model") {
        assert(!sent && transport.bytes.empty());
        // Rejection must not consume the request ID or leave a partial frame.
        assert(api.subscribe(&transport, FiveTratumModels::OctaxeGammaMiningAgent, "BM1370", 0));
        assert(transport.bytes.find("\"id\": 1") != std::string::npos);
    } else if (scenario == "unsafe" || scenario == "disconnected" || scenario == "send-failure") {
        assert(!sent && transport.bytes.empty());
        if (scenario == "unsafe") {
            assert(api.subscribe(&transport, "NerdQAxe++", "BM1370", 0));
            assert(transport.bytes.find("\"id\": 1") != std::string::npos);
        } else {
            std::cout << "{\"sent\":false}";
            return 0;
        }
    } else assert(sent);
    std::cout << transport.bytes;
}
