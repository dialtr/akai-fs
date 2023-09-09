// Copyright (C) 2023 The akai-fs authors. All rights reserved.
#include "fuse_operations.h"

int afs_getattr(const char* path, struct stat* info, struct fuse_file_info* fi) {
  return -EPERM;
}
