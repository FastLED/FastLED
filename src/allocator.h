
#pragma once

#include "namespace.h"
#include <stddef.h>

FASTLED_NAMESPACE_BEGIN

void SetLargeBlockAllocator(void *(*alloc)(size_t), void (*free)(void *));
void* LargeBlockAllocate(size_t size);
void LargeBlockDeallocate(void* ptr);

FASTLED_NAMESPACE_END
