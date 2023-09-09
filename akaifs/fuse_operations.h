// Copyright (C) 2023 The akai-fs authors. All rights reserved.

#ifndef AKAI_FS_AKAIFS_FUSE_OPERATIONS_H_
#define AKAI_FS_AKAIFS_FUSE_OPERATIONS_H_

#define FUSE_USE_VERSION 35

#include <errno.h>
#include <fuse3/fuse.h>


int afs_getattr(const char* path, struct stat* info, struct fuse_file_info* fi);


#endif // AKAI_FS_AKAIFS_FUSE_OPERATIONS_H_
