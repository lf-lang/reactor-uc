#include <stdio.h>
#include <stdbool.h>
#include "coap3/coap.h"

#define COAP_LISTEN_UCAST_IP "127.0.0.1"

void resource_handler_hello(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                            const coap_string_t *query, coap_pdu_t *response) {
  const coap_address_t *remote_addr = coap_session_get_addr_remote(session);
  
  char addr_str[INET6_ADDRSTRLEN + 8] = {0};
  if (remote_addr) {
    coap_print_addr(remote_addr, (uint8_t *)addr_str, sizeof(addr_str));
    printf("Request from: %s\n", addr_str);
  }
  coap_show_pdu(COAP_LOG_WARN, request);
  coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
  coap_add_data(response, 5, (const uint8_t *)"world");
  coap_show_pdu(COAP_LOG_WARN, response);
}

void resource_handler_hello_my(coap_resource_t *resource, coap_session_t *session, const coap_pdu_t *request,
                               const coap_string_t *query, coap_pdu_t *response) {
  coap_show_pdu(COAP_LOG_WARN, request);
  coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
  coap_add_data(response, 8, (const uint8_t *)"my world");
  coap_show_pdu(COAP_LOG_WARN, response);
}

int main(void) {
  coap_context_t *ctx = NULL;
  coap_resource_t *resource = NULL;
  int result = EXIT_FAILURE;

  uint32_t scheme_hint_bits;
  coap_addr_info_t *info = NULL;
  coap_addr_info_t *info_list = NULL;
  coap_str_const_t *my_address = coap_make_str_const(COAP_LISTEN_UCAST_IP);
  bool have_ep = false;

  /* Initialize libcoap library */
  coap_startup();

  /* Set logging level */
  coap_set_log_level(COAP_LOG_WARN);

  /* Create CoAP context */
  ctx = coap_new_context(NULL);
  if (!ctx) {
    coap_log_emerg("cannot initialize context\n");
    goto finish;
  }

  /* Let libcoap do the multi-block payload handling (if any) */
  coap_context_set_block_mode(ctx, COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);

  scheme_hint_bits = coap_get_available_scheme_hint_bits(0, 0, COAP_PROTO_UDP);
  info_list = coap_resolve_address_info(my_address, 0, 0, 0, 0, 0, scheme_hint_bits, COAP_RESOLVE_TYPE_LOCAL);
  /* Create CoAP listening endpoint(s) */
  for (info = info_list; info != NULL; info = info->next) {
    coap_endpoint_t *ep;

    ep = coap_new_endpoint(ctx, &info->addr, info->proto);
    if (!ep) {
      coap_log_warn("cannot create endpoint for CoAP proto %u\n", info->proto);
    } else {
      have_ep = true;
    }
  }
  coap_free_address_info(info_list);
  if (have_ep == false) {
    coap_log_err("No context available for interface '%s'\n", (const char *)my_address->s);
    goto finish;
  }

  /* Create a resource that the server can respond to with information */
  resource = coap_resource_init(coap_make_str_const("hello"), 0);
  coap_register_handler(resource, COAP_REQUEST_GET, resource_handler_hello);
  coap_add_resource(ctx, resource);

  /* Create another resource that the server can respond to with information */
  resource = coap_resource_init(coap_make_str_const("hello/my"), 0);
  coap_register_handler(resource, COAP_REQUEST_GET, resource_handler_hello_my);
  coap_add_resource(ctx, resource);

  /* Handle any libcoap I/O requirements */
  while (true) {
    coap_io_process(ctx, COAP_IO_WAIT);
  }

  result = EXIT_SUCCESS;
finish:

  coap_free_context(ctx);
  coap_cleanup();

  return result;
}