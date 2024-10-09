#include "reactor-uc/reactor-uc.h"
#include <pthread.h>
#include <sys/socket.h>

// Reactor Sender
typedef struct {
  Timer super;
  Reaction *effects[1];
} Timer1;

typedef struct {
  Reaction super;
} Reaction1;

typedef struct {
  Output super;
  Reaction *sources[1];
  interval_t value;
} Out;

typedef struct {
  Reactor super;
  Reaction1 reaction;
  Timer1 timer;
  Out out;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Sender;

void timer_handler(Reaction *_self) {
  Sender *self = (Sender *)_self->parent;
  Environment *env = self->super.env;
  Out *out = &self->out;

  printf("Timer triggered @ %ld\n", env->get_elapsed_logical_time(env));
  lf_set(out, env->get_elapsed_logical_time(env));
}

void Reaction1_ctor(Reaction1 *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, timer_handler, NULL, 0, 0);
}

void Out_ctor(Out *self, Sender *parent) {
  self->sources[0] = &parent->reaction.super;
  Output_ctor(&self->super, &parent->super, self->sources, 1);
}

void Sender_ctor(Sender *self, Reactor *parent, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->timer;
  Reactor_ctor(&self->super, "Sender", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Reaction1_ctor(&self->reaction, &self->super);
  Timer_ctor(&self->timer.super, &self->super, 0, MSEC(100), self->timer.effects, 1);
  Out_ctor(&self->out, self);
  TIMER_REGISTER_EFFECT(self->timer, self->reaction);

  // Register reaction as a source for out
  OUTPUT_REGISTER_SOURCE(self->out, self->reaction);
}

// Reactor Receiver
typedef struct {
  Reaction super;
} Reaction2;

typedef struct {
  Input super;
  interval_t buffer[1];
  Reaction *effects[1];
} In;

typedef struct {
  Reactor super;
  Reaction2 reaction;
  In inp;
  int cnt;
  Reaction *_reactions[1];
  Trigger *_triggers[1];
} Receiver;

void In_ctor(In *self, Receiver *parent) {
  Input_ctor(&self->super, &parent->super, self->effects, 1, self->buffer, sizeof(self->buffer[0]));
}

void input_handler(Reaction *_self) {
  Receiver *self = (Receiver *)_self->parent;
  Environment *env = self->super.env;
  In *inp = &self->inp;

  printf("Input triggered @ %ld with %ld\n", env->get_elapsed_logical_time(env), lf_get(inp));
}

void Reaction2_ctor(Reaction2 *self, Reactor *parent) {
  Reaction_ctor(&self->super, parent, input_handler, NULL, 0, 0);
}

void Receiver_ctor(Receiver *self, Reactor *parent, Environment *env) {
  self->_reactions[0] = (Reaction *)&self->reaction;
  self->_triggers[0] = (Trigger *)&self->inp;
  Reactor_ctor(&self->super, "Receiver", env, parent, NULL, 0, self->_reactions, 1, self->_triggers, 1);
  Reaction2_ctor(&self->reaction, &self->super);
  In_ctor(&self->inp, self);

  // Register reaction as an effect of in
  INPUT_REGISTER_EFFECT(self->inp, self->reaction);
}

typedef struct {
  FederatedOutputConnection super;
  interval_t buffer[1];
} ConnSender;

void ConnSender_ctor(ConnSender *self, FederatedConnectionBundle *bundle, Port *upstream) {
  FederatedOutputConnection_ctor(&self->super, bundle, 0, upstream, &self->buffer[0], sizeof(self->buffer[0]));
}

typedef struct {
  FederatedConnectionBundle super;
  TcpIpBundle bundle;
  ConnSender conn;
  FederatedOutputConnection *output[1];
} SenderRecvBundle;

void SenderRecvConn_ctor(SenderRecvBundle *self, Sender *parent) {
  TcpIpBundle_ctor(&self->bundle, "127.0.0.1", 8900, AF_INET);
  ConnSender_ctor(&self->conn, &self->super, &parent->out.super.super);

  TcpIpBundle *bundle = &self->bundle;
  bundle->change_block_state(bundle, false);
  lf_ret_t ret = LF_ERR;
  do {
    ret = bundle->bind(bundle);
    if (ret != LF_OK) {
      printf("ret2=%d\n", ret);
    }

  } while (ret != LF_OK);

  // accept one connection
  bool new_connection;
  do {
    new_connection = bundle->accept(bundle);
  } while (!new_connection);
  printf("Connection accepted\n");

  FederatedConnectionBundle_ctor(&self->super, &parent->super, &self->bundle, NULL, 0,
                                 (FederatedOutputConnection **)&self->output, 1);
}

typedef struct {
  FederatedInputConnection super;
  interval_t buffer[5];
  Input *downstreams[1];
} ConnRecv;

void ConnRecv_ctor(ConnRecv *self, Reactor *parent) {
  FederatedInputConnection_ctor(&self->super, parent, MSEC(100), false, (Port **)&self->downstreams, 1,
                                &self->buffer[0], sizeof(self->buffer[0]), 5);
}

typedef struct {
  FederatedConnectionBundle super;
  TcpIpBundle bundle;
  ConnRecv conn;
  FederatedInputConnection *inputs[1];
} RecvSenderBundle;

void RecvSenderBundle_ctor(RecvSenderBundle *self, Reactor *parent) {
  ConnRecv_ctor(&self->conn, parent);
  TcpIpBundle_ctor(&self->bundle, "127.0.0.1", 8900, AF_INET);

  TcpIpBundle *bundle = &self->bundle;
  bundle->change_block_state(bundle, false);
  lf_ret_t ret = LF_ERR;
  do {
    ret = bundle->connect(bundle);
  } while (ret != LF_OK);

  FederatedConnectionBundle_ctor(&self->super, parent, &self->bundle, (FederatedInputConnection **)&self->inputs, 1,
                                 NULL, 0);
}

// Reactor main
struct MainSender {
  Reactor super;
  Sender sender;
  SenderRecvBundle bundle;

  Reactor *_children[1];
};

struct MainRecv {
  Reactor super;
  Receiver receiver;
  RecvSenderBundle bundle;

  Reactor *_children[1];
};

void MainSender_ctor(struct MainSender *self, Environment *env) {
  self->_children[0] = &self->sender.super;
  Sender_ctor(&self->sender, &self->super, env);

  SenderRecvConn_ctor(&self->bundle, &self->sender);
  Reactor_ctor(&self->super, "MainSender", env, NULL, self->_children, 1, NULL, 0, NULL, 0);
}

void MainRecv_ctor(struct MainRecv *self, Environment *env) {
  self->_children[0] = &self->receiver.super;
  Receiver_ctor(&self->receiver, &self->super, env);

  RecvSenderBundle_ctor(&self->bundle, &self->super);

  CONN_REGISTER_DOWNSTREAM(self->bundle.conn, self->receiver.inp);
  Reactor_ctor(&self->super, "MainRecv", env, NULL, self->_children, 1, NULL, 0, NULL, 0);
}

void *main_sender(void *unused) {
  (void)unused;
  struct MainSender main;
  Environment env;
  Environment_ctor(&env, (Reactor *)&main);
  MainSender_ctor(&main, &env);
  env.set_timeout(&env, SEC(1));
  env.assemble(&env);
  env.start(&env);
  return NULL;
}

void *main_recv(void *unused) {
  (void)unused;
  struct MainRecv main;
  Environment env;
  Environment_ctor(&env, (Reactor *)&main);
  MainRecv_ctor(&main, &env);
  env.set_timeout(&env, SEC(1));
  env.keep_alive = true;
  env.assemble(&env);
  env.start(&env);
  return NULL;
}

int main() {
  pthread_t thread1;
  pthread_t thread2;

  // Create the first thread running func1
  if (pthread_create(&thread1, NULL, main_recv, NULL)) {
    printf("Error creating thread 1\n");
    return 1;
  }

  // Create the second thread running func2
  if (pthread_create(&thread2, NULL, main_sender, NULL)) {
    printf("Error creating thread 2\n");
    return 1;
  }

  // Wait for both threads to finish
  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);

  printf("Both threads have finished\n");
  return 0;
}
