#ifndef REACTOR_C_H
#define REACTOR_C_H

#define lf_print(str, ...) printf(str, __VA_ARGS__)
#define PRINTF_TIME "%" PRId64
#define PRINTF_MICROSTEP "%" PRIu32
#define PRINTF_TAG "(" PRINTF_TIME ", " PRINTF_MICROSTEP ")"

#define lf_time_logical_elapsed() env->get_elapsed_logical_time(env)
#define lf_time_logical() env->get_logical_time(env)
#define lf_tag() env->get_logical_time(env)
#define lf_time_physical() env->get_physical_time(env)
#define lf_time_physical_elapsed() env->get_elapsed_physical_time(env)

#define lf_request_stop() env->request_shutdown(env)

#define lf_schedule_token(action, offset, val)                                                                         \
  do {                                                                                                                 \
    __typeof__(val) __val = (val);                                                                                     \
    lf_ret_t ret = (action)->super.schedule(&(action)->super, (offset), (const void *)&__val);                         \
    if (ret == LF_FATAL) {                                                                                             \
      LF_ERR(TRIG, "Scheduling an value, that doesn't have value!");                                                   \
      Scheduler *sched = (action)->super.super.parent->env->scheduler;                                                 \
      sched->do_shutdown(sched, sched->current_tag(sched));                                                            \
      throw("Tried to schedule a value onto an action without a type!");                                               \
    }                                                                                                                  \
  } while (0)

#define lf_set_token(action, val)                                                                                      \
  do {                                                                                                                 \
    __typeof__(val) __val = (val);                                                                                     \
    Port *_port = (Port *)(port);                                                                                      \
    _port->set(_port, &__val);                                                                                         \
  } while (0)

#warning "You are using the deprecated reactor-c interface"

#endif // REACTOR_C_H
