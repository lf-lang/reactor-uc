/*
 * Linker snippet for the Zephyr CoAP network channel (coap_udp_ip_channel.c).
 *
 * The channel registers its CoAP resources (/connect, /disconnect, /message) with
 * COAP_RESOURCE_DEFINE on the service `lf_coap_service`. Zephyr places these resources in an
 * iterable section that must be provided explicitly. reactor-uc's CMakeLists.txt adds this snippet
 * automatically for Zephyr builds with CONFIG_COAP_SERVER=y.
 */
#include <zephyr/linker/iterable_sections.h>

ITERABLE_SECTION_RAM(coap_resource_lf_coap_service, Z_LINK_ITERABLE_SUBALIGN)
