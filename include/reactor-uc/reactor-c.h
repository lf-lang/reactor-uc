#ifndef REACTOR_C_H
#define REACTOR_C_H

#define lf_print(...)                                                                                                  \
  do {                                                                                                                 \
    printf(__VA_ARGS__);                                                                                               \
    printf("\n");                                                                                                      \
  } while (0)

#define lf_time_start() env->scheduler->start_time
#define lf_time_logical_elapsed() env->get_elapsed_logical_time(env)
#define lf_time_logical() env->get_logical_time(env)
#define lf_time_physical() env->get_physical_time(env)
#define lf_time_physical_elapsed() env->get_elapsed_physical_time(env)
#define lf_tag() env->scheduler->current_tag(env->scheduler)
#define lf_sleep(duration) env->wait_for(env, duration)
#define lf_nanosleep(duration) lf_sleep(duration)

#define lf_request_stop() env->request_shutdown(env)

#define lf_schedule_token(action, offset, val)                                                                         \
  do {                                                                                                                 \
    __typeof__(val) __val = (val);                                                                                     \
    lf_ret_t ret = (action)->super.schedule(&(action)->super, (offset), (const void *)&__val);                         \
    if (ret == LF_FATAL) {                                                                                             \
      LF_ERR(TRIG, "Scheduling an value, that doesn't have value!");                                                   \
      throw("Tried to schedule a value onto an action without a type!");                                               \
    }                                                                                                                  \
  } while (0)

#define lf_schedule_int(action, offset, val) lf_schedule(action, offset, val)

#define lf_set_token(action, val)                                                                                      \
  do {                                                                                                                 \
    __typeof__(val) __val = (val);                                                                                     \
    Port *_port = (Port *)(port);                                                                                      \
    _port->set(_port, &__val);                                                                                         \
  } while (0)

#define lf_print_error_and_exit(...)                                                                                   \
  do {                                                                                                                 \
    printf("ERROR: ");                                                                                                 \
    printf(__VA_ARGS__);                                                                                               \
    exit(1);                                                                                                           \
  } while (0)

#define lf_print_warning(...) printf(__VA_ARGS__)
#warning "You are using the deprecated reactor-c interface"

#endif // REACTOR_C_H
