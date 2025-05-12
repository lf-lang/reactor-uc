#include <stdio.h>
#include "coap3/coap.h"

static int have_response = 0;

#ifndef COAP_CLIENT_URI
#define COAP_CLIENT_URI "coap://coap.me/hello"
#endif

int resolve_address(coap_str_const_t *host, uint16_t port, coap_address_t *dst, int scheme_hint_bits) {
  int ret = 0;
  coap_addr_info_t *addr_info;

  addr_info =
      coap_resolve_address_info(host, port, port, port, port, AF_UNSPEC, scheme_hint_bits, COAP_RESOLVE_TYPE_REMOTE);
  if (addr_info) {
    ret = 1;
    *dst = addr_info->addr;
  }

  coap_free_address_info(addr_info);
  return ret;
}

int main(int argc, char *argv[]) {
  coap_context_t *ctx = NULL;
  coap_session_t *session = NULL;
  coap_optlist_t *optlist = NULL;
  coap_address_t dst;
  coap_pdu_t *pdu = NULL;
  int result = EXIT_FAILURE;

  int len;
  int res;
  unsigned int wait_ms;
  coap_uri_t uri;
  const char *coap_uri = COAP_CLIENT_URI;
  int is_mcast;
#define BUFSIZE 100
  unsigned char scratch[BUFSIZE];

  /* Support run-time defining of CoAP URIs */
  if (argc > 1) {
    coap_uri = argv[1];
  }

  /* Initialize libcoap library */
  coap_startup();

  /* Set logging level */
  coap_set_log_level(COAP_LOG_WARN);

  /* Parse the URI */
  len = coap_split_uri((const unsigned char *)coap_uri, strlen(coap_uri), &uri);
  if (len != 0) {
    coap_log_warn("Failed to parse uri %s\n", coap_uri);
    goto finish;
  }

  /* resolve destination address where server should be sent */
  len = resolve_address(&uri.host, uri.port, &dst, 1 << uri.scheme);
  if (len <= 0) {
    coap_log_warn("Failed to resolve address %*.*s\n", (int)uri.host.length, (int)uri.host.length,
                  (const char *)uri.host.s);
    goto finish;
  }
  is_mcast = coap_is_mcast(&dst);

  /* create CoAP context and a client session */
  if (!(ctx = coap_new_context(NULL))) {
    coap_log_emerg("cannot create libcoap context\n");
    goto finish;
  }
  /* Support large responses */
  coap_context_set_block_mode(ctx, COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);

  if (uri.scheme == COAP_URI_SCHEME_COAP) {
    session = coap_new_client_session(ctx, NULL, &dst, COAP_PROTO_UDP);
  } else if (uri.scheme == COAP_URI_SCHEME_COAP_TCP) {
    session = coap_new_client_session(ctx, NULL, &dst, COAP_PROTO_TCP);
  }
  if (!session) {
    coap_log_emerg("cannot create client session\n");
    goto finish;
  }

  /* construct CoAP message */
  pdu = coap_pdu_init(is_mcast ? COAP_MESSAGE_NON : COAP_MESSAGE_CON, COAP_REQUEST_CODE_GET,
                      coap_new_message_id(session), coap_session_max_pdu_size(session));
  if (!pdu) {
    coap_log_emerg("cannot create PDU\n");
    goto finish;
  }

  /* Add option list (which will be sorted) to the PDU */
  len = coap_uri_into_options(&uri, &dst, &optlist, 1, scratch, sizeof(scratch));
  if (len) {
    coap_log_warn("Failed to create options\n");
    goto finish;
  }

  if (optlist) {
    res = coap_add_optlist_pdu(pdu, &optlist);
    if (res != 1) {
      coap_log_warn("Failed to add options to PDU\n");
      goto finish;
    }
  }

  coap_show_pdu(COAP_LOG_WARN, pdu);

  /* and send the PDU */
  if (coap_send(session, pdu) == COAP_INVALID_MID) {
    coap_log_err("cannot send CoAP pdu\n");
    goto finish;
  }

  wait_ms = (coap_session_get_default_leisure(session).integer_part + 1) * 1000;

  while (have_response == 0 || is_mcast) {
    res = coap_io_process(ctx, 1000);
    if (res >= 0) {
      if (wait_ms > 0) {
        if ((unsigned)res >= wait_ms) {
          printf("timeout\n");
          break;
        } else {
          wait_ms -= res;
        }
      }
    }
  }

  result = EXIT_SUCCESS;
finish:
  coap_delete_optlist(optlist);
  coap_session_release(session);
  coap_free_context(ctx);
  coap_cleanup();

  return result;
}