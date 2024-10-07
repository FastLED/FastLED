#pragma once


#include <stdint.h>
#include <stddef.h>
#include "namespace.h"
#include "filehandle.h"


FASTLED_NAMESPACE_BEGIN


class FileReader {
  public:
    struct Visitor {
      virtual void accept(const char* path) = 0;
    };
  
    virtual FileHandlePtr openRead(const char *path) = 0;
    virtual void close(FileHandlePtr file) = 0;

    // False signals to stop the file iteration.
    virtual bool ls(Visitor &visitor) = 0;
};


FASTLED_NAMESPACE_END
