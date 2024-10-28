#pragma once

#include "ptr.h"
#include "fx/storage/fs.h"

inline FsImplPtr FsImpl::New(int cs_pin) {
    return FsImplPtr::Null();
}