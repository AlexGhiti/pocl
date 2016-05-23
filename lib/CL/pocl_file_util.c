#if !defined(_MSC_VER) && !defined(__MINGW32__)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#else
#include <io.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "pocl.h"
#include "pocl_file_util.h"

/*****************************************************************************/

int
pocl_rm_rf(const char* path) 
{
  DIR *d = opendir(path);
  size_t path_len = strlen(path);
  int error = -1;
  
  if(d) 
    {
      struct dirent *p = readdir(d);
      error = 0;
      while (!error && p)
        {
          char *buf;
          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
            continue;
          
          size_t len = path_len + strlen(p->d_name) + 2;
          buf = malloc(len);
          buf[len] = '\0';
          if (buf)
            {
              struct stat statbuf;
              snprintf(buf, len, "%s/%s", path, p->d_name);
              
              if (!stat(buf, &statbuf))
                error = pocl_rm_rf(buf);
              else 
                error = remove(buf);
              
              free(buf);
            }
          p = readdir(d);
        }
      closedir(d);
      
      if (!error)
        remove(path);
    }
  return error;
}


int
pocl_mkdir_p (const char* path)
{
  int error;
  int errno_tmp;
  error = mkdir (path, S_IRWXU);
  if (error && errno == ENOENT)
    { // creates all needed directories recursively
      char *previous_path;
      int previous_path_length = strrchr (path, '/') - path;
      previous_path = malloc (previous_path_length + 1);
      strncpy (previous_path, path, previous_path_length);
      previous_path[previous_path_length] = '\0';
      pocl_mkdir_p ((const char*) previous_path);
      free (previous_path);
      error = mkdir (path, S_IRWXU);
    }
  else if (error && errno == EEXIST)
    error = 0;

  return error;
}

int
pocl_remove(const char* path) 
{
  return remove(path);
}

int
pocl_exists(const char* path) 
{
  return !access(path, R_OK);
}

int
pocl_filesize(const char* path, uint64_t* res) 
{
  FILE *f = fopen(path, "r");
  if (f == NULL)
    return -1;
  
  fseek(f, 0, SEEK_END);
  *res = ftell(f);
  
  fclose(f);
  return 0;
}

int 
pocl_touch_file(const char* path) 
{
  FILE *f = fopen(path, "w");
  if (f)
    {
      fclose(f);
      return 0;
    }
  return -1;
}
/****************************************************************************/

int
pocl_read_file(const char* path, char** content, uint64_t *filesize) 
{
  assert(content);
  assert(path);
  assert(filesize);
  
  *content = NULL;
  
  int errcode = pocl_filesize(path, filesize);
  ssize_t fsize = (ssize_t)(*filesize);
  if (!errcode) 
    {
      FILE *f = fopen(path, "r");
      
      *content = (char*)malloc(fsize+1);
      
      size_t rsize = fread(*content, 1, fsize, f);
      (*content)[rsize] = '\0';
      if (rsize < (size_t)fsize) 
        {
          errcode = -1;
          fclose(f);
        } 
      else 
        {
          if (fclose(f))
            errcode = -1;
        }
    }
  return errcode;
}



int 
pocl_write_file(const char *path, const char* content,
                uint64_t    count,
                int         append,
                int         dont_rewrite) 
{
  assert(path);
  assert(content);
  
  if (pocl_exists(path)) 
    {
      if (dont_rewrite) 
        {
          if (!append)
            return 0;
        } 
      else 
        {
          int res = pocl_remove(path);
          if (res)
            return res;
        }
    }
  
  FILE *f;
  if (append)
    f = fopen(path, "a");
  else
    f = fopen(path, "w");
  
  if (f == NULL)
    return -1;
  
  if (fwrite(content, 1, (size_t)count, f) < (size_t)count)
    return -1;
  
  return fclose(f);
}


/****************************************************************************/

static void* 
acquire_lock_internal(const char* path, int shared) 
{
  /* Can't return value that compares equal to NULL */
  return (void*)4096;
}


void* 
acquire_lock(const char *path, int shared) 
{
    return acquire_lock_internal(path, shared);
}


void release_lock(void* lock) 
{
    return;
}
