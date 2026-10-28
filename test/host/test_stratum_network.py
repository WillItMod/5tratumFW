#!/usr/bin/env python3
"""Exercise the actual pool resolver with deterministic interface/DNS stubs."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class StratumNetworkTests(unittest.TestCase):
    def test_link_local_scope_follows_default_uplink(self):
        source = (ROOT / "components/stratum/stratum_socket.c").read_text()
        # Keep the production resolver verbatim. Socket option setup needs a
        # device transport and is unrelated to selecting an IPv6 interface.
        resolver = source[:source.index("\nvoid stratum_socket_set_options(")]
        with tempfile.TemporaryDirectory(prefix="gamma-stratum-network-") as directory:
            temp = Path(directory)
            (temp / "resolver.c").write_text(resolver)
            (temp / "esp_err.h").write_text("typedef int esp_err_t;\n#define ESP_OK 0\n"
                "#define ESP_ERR_INVALID_ARG 1\n#define ESP_ERR_NOT_FOUND 2\n#define ESP_ERR_NOT_SUPPORTED 3\n")
            (temp / "esp_transport.h").write_text("typedef void *esp_transport_handle_t;\n")
            (temp / "esp_log.h").write_text("#define ESP_LOGD(tag,...) ((void)(tag))\n"
                "#define ESP_LOGI ESP_LOGD\n#define ESP_LOGW ESP_LOGD\n#define ESP_LOGE ESP_LOGD\n")
            (temp / "esp_netif.h").write_text("typedef struct {int index;} esp_netif_t;\n"
                "esp_netif_t *esp_netif_get_default_netif(void);\n"
                "int esp_netif_get_netif_impl_index(esp_netif_t *netif);\n")
            (temp / "lwip").mkdir()
            (temp / "lwip/sockets.h").write_text("#include <sys/socket.h>\n#include <arpa/inet.h>\n")
            (temp / "lwip/netdb.h").write_text("#include <netdb.h>\n")
            (temp / "harness.c").write_text(r'''
#include "stratum_socket.h"
#include "esp_netif.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
static esp_netif_t selected = {.index=7};
static bool have_default=true;
static unsigned default_reads, dns_frees;
static struct sockaddr_storage address;
static struct addrinfo answer;
esp_netif_t *esp_netif_get_default_netif(void) { default_reads++; return have_default ? &selected : NULL; }
int esp_netif_get_netif_impl_index(esp_netif_t *netif) { assert(netif==&selected); return netif->index; }
int getaddrinfo(const char *host,const char *service,const struct addrinfo *hints,struct addrinfo **result)
{
    assert(!strcmp(host,"fixture.invalid") && !strcmp(service,"3333") && hints->ai_family==AF_UNSPEC);
    *result=&answer; return 0;
}
void freeaddrinfo(struct addrinfo *result) { assert(result==&answer); dns_frees++; }
static void ipv6(const char *text,unsigned scope)
{
    memset(&address,0,sizeof(address));
    struct sockaddr_in6 *value=(struct sockaddr_in6*)&address;
    value->sin6_family=AF_INET6; value->sin6_scope_id=scope;
    assert(inet_pton(AF_INET6,text,&value->sin6_addr)==1);
    answer=(struct addrinfo){.ai_family=AF_INET6,.ai_addr=(struct sockaddr*)value,.ai_addrlen=sizeof(*value)};
}
static stratum_connection_info_t resolve(void)
{
    stratum_connection_info_t value;
    assert(stratum_socket_resolve("fixture.invalid",3333,&value)==ESP_OK);
    return value;
}
int main(void)
{
    ipv6("fe80::1234",0);
    stratum_connection_info_t value=resolve();
    assert(((struct sockaddr_in6*)&value.dest_addr)->sin6_scope_id==7 && !strcmp(value.host_ip,"fe80::1234%7"));
    selected.index=3; value=resolve(); // A different selected uplink, without a fixed Wi-Fi key.
    assert(((struct sockaddr_in6*)&value.dest_addr)->sin6_scope_id==3 && !strcmp(value.host_ip,"fe80::1234%3"));
    ipv6("fe80::1234",11); unsigned reads=default_reads; value=resolve();
    assert(((struct sockaddr_in6*)&value.dest_addr)->sin6_scope_id==11 && default_reads==reads);
    ipv6("2001:db8::1234",0); value=resolve();
    assert(((struct sockaddr_in6*)&value.dest_addr)->sin6_scope_id==0 && default_reads==reads);
    ipv6("fe80::1234",0); have_default=false; value=resolve();
    assert(((struct sockaddr_in6*)&value.dest_addr)->sin6_scope_id==0);
    have_default=true; selected.index=0; value=resolve();
    assert(((struct sockaddr_in6*)&value.dest_addr)->sin6_scope_id==0 && !strchr(value.host_ip,'%'));
    selected.index=-1; value=resolve();
    assert(((struct sockaddr_in6*)&value.dest_addr)->sin6_scope_id==0 && !strchr(value.host_ip,'%'));
    struct sockaddr_in *v4=(struct sockaddr_in*)&address;
    memset(&address,0,sizeof(address)); v4->sin_family=AF_INET;
    assert(inet_pton(AF_INET,"192.0.2.10",&v4->sin_addr)==1);
    answer=(struct addrinfo){.ai_family=AF_INET,.ai_addr=(struct sockaddr*)v4,.ai_addrlen=sizeof(*v4)};
    reads=default_reads; value=resolve();
    assert(value.addr_family==AF_INET && !strcmp(value.host_ip,"192.0.2.10") && default_reads==reads && dns_frees==8);
    assert(stratum_socket_resolve(NULL,3333,&value)==ESP_ERR_INVALID_ARG);
    assert(stratum_socket_resolve("fixture.invalid",0,&value)==ESP_ERR_INVALID_ARG);
    puts("PASS"); return 0;
}
''')
            binary = temp / "resolver-test"
            command = shlex.split(os.environ.get("CC", "cc")) + [
                "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                "-fno-omit-frame-pointer", "-g", "-I", str(temp),
                "-I", str(ROOT / "components/stratum/include"),
                str(temp / "resolver.c"), str(temp / "harness.c"), "-o", str(binary)]
            result = subprocess.run(command, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(binary)], text=True, capture_output=True,
                env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1", "UBSAN_OPTIONS": "halt_on_error=1"})
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.strip(), "PASS")


if __name__ == "__main__":
    unittest.main(verbosity=2)
