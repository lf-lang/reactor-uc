#include "reactor-uc/extension.h"

#if defined(LF_RUNTIME_EXTENSIONS)

#include "reactor-uc/environment.h"
#include "reactor-uc/logging.h"

void LfExtension_ctor(LfExtension* self, const LfExtensionDescriptor* descriptor, void* state) {
  validate(self);
  self->descriptor = descriptor;
  self->state = state;
  self->_owner = NULL;
  self->_next = NULL;
}

static bool descriptor_is_supported(const LfExtensionDescriptor* descriptor) {
  if (descriptor == NULL || descriptor->api_version != LF_RUNTIME_EXTENSION_API_VERSION ||
      descriptor->struct_size < sizeof(LfExtensionDescriptor) || descriptor->name == NULL ||
      (descriptor->required_capabilities & ~LF_RUNTIME_EXTENSION_CAPABILITIES) != 0) {
    return false;
  }
  return true;
}

lf_ret_t Environment_register_extension(Environment* self, LfExtension* extension) {
  if (self == NULL || extension == NULL || !descriptor_is_supported(extension->descriptor)) {
    return LF_INVALID_VALUE;
  }
  if (extension->_owner != NULL && extension->_owner != self) {
    return LF_INVALID_VALUE;
  }

  for (LfExtension* it = self->_extensions_head; it != NULL; it = it->_next) {
    if (it == extension) {
      return LF_OK;
    }
  }

  if (extension->_owner != NULL || self->_extensions_sealed) {
    return LF_INVALID_VALUE;
  }

  extension->_owner = self;
  extension->_next = NULL;
  LfExtension** slot = &self->_extensions_head;
  while (*slot != NULL) {
    slot = &(*slot)->_next;
  }
  *slot = extension;

  LF_DEBUG(ENV, "Registered extension %s at %p", extension->descriptor->name, (void*)extension);
  return LF_OK;
}

void Environment_notify_tag_start(Environment* self, tag_t tag, bool is_shutdown) {
  self->_extensions_sealed = true;
  const LfExtensionTagContext context = {.environment = self, .tag = tag, .is_shutdown = is_shutdown};
  for (LfExtension* extension = self->_extensions_head; extension != NULL; extension = extension->_next) {
    if (extension->descriptor->on_tag_start != NULL) {
      extension->descriptor->on_tag_start(extension->state, &context);
    }
  }
}

void Environment_notify_tag_complete(Environment* self, tag_t tag, bool is_shutdown) {
  self->_extensions_sealed = true;
  if (lf_tag_compare(tag, self->_last_extension_tag) == 0) {
    return;
  }
  self->_last_extension_tag = tag;
  const LfExtensionTagContext context = {.environment = self, .tag = tag, .is_shutdown = is_shutdown};
  for (LfExtension* extension = self->_extensions_head; extension != NULL; extension = extension->_next) {
    if (extension->descriptor->on_tag_complete != NULL) {
      extension->descriptor->on_tag_complete(extension->state, &context);
    }
  }
}

void Environment_notify_shutdown(Environment* self) {
  self->_extensions_sealed = true;
  if (self->_extensions_shutdown_notified) {
    return;
  }
  self->_extensions_shutdown_notified = true;
  for (LfExtension* extension = self->_extensions_head; extension != NULL; extension = extension->_next) {
    if (extension->descriptor->on_shutdown != NULL) {
      extension->descriptor->on_shutdown(extension->state, self);
    }
  }
}

void LfGate_ctor(LfGate* self) {
  validate(self);
  self->_head = NULL;
}

void LfGateCondition_ctor(LfGateCondition* self) {
  validate(self);
  self->value = NULL;
  self->_next = NULL;
  self->_owner = NULL;
}

lf_ret_t LfGate_add(LfGate* self, LfGateCondition* condition, const bool* value) {
  if (self == NULL || condition == NULL || value == NULL) {
    return LF_INVALID_VALUE;
  }
  if (condition->_owner != NULL && condition->_owner != self) {
    return LF_INVALID_VALUE;
  }
  if (condition->_owner == self) {
    return condition->value == value ? LF_OK : LF_INVALID_VALUE;
  }

  condition->value = value;
  condition->_owner = self;
  condition->_next = NULL;
  LfGateCondition** slot = &self->_head;
  while (*slot != NULL) {
    slot = &(*slot)->_next;
  }
  *slot = condition;
  return LF_OK;
}

bool LfGate_contains(const LfGate* self, const bool* value) {
  validate(self);
  for (const LfGateCondition* condition = self->_head; condition != NULL; condition = condition->_next) {
    if (condition->value == value) {
      return true;
    }
  }
  return false;
}

void LfExtensionBinding_ctor(LfExtensionBinding* self) {
  validate(self);
  self->descriptor = NULL;
  self->state = NULL;
  self->_owner = NULL;
  self->_next = NULL;
}

#endif /* LF_RUNTIME_EXTENSIONS */
