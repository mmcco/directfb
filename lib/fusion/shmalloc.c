/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrj�l� <syrjala@sci.fi>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <direct/build.h>
#include <direct/debug.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <fusion/build.h>
#include <fusion/shmalloc.h>


#if FUSION_BUILD_MULTI

#include <fusion/shm/shm_internal.h>

/*************************** MULTI APPLICATION CORE ***************************/

#if DIRECT_BUILD_DEBUG

void
fusion_dbg_print_memleaks( FusionSHMPoolShared *pool )
{
     DirectResult  ret;
     SHMemDesc    *desc;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return;
     }

     if (pool->allocs) {
          D_DEBUG( "Shared memory allocations remaining (%d): \n",
                   direct_list_count_elements_EXPENSIVE( pool->allocs ) );

          direct_list_foreach (desc, pool->allocs)
               D_DEBUG( "%7d bytes at %p allocated in %s (%s: %u)\n",
                        desc->bytes, desc->mem, desc->func, desc->file, desc->line );
     }

     fusion_skirmish_dismiss( &pool->lock );
}

static SHMemDesc *
fill_shmem_desc( SHMemDesc *desc, int bytes, const char *func, const char *file, int line )
{
     D_ASSERT( desc != NULL );

     desc->mem   = desc + 1;
     desc->bytes = bytes;

     snprintf( desc->func, SHMEMDESC_FUNC_NAME_LENGTH, func );
     snprintf( desc->file, SHMEMDESC_FILE_NAME_LENGTH, file );

     desc->line = line;

     return desc;
}

/* Allocate SIZE bytes of memory.  */
void *
fusion_dbg_shmalloc( FusionSHMPoolShared *pool,
                     const char *file, int line,
                     const char *func, size_t __size )
{
     DirectResult  ret;
     SHMemDesc    *desc;
     void         *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( __size > 0 );

     D_HEAVYDEBUG("Fusion/SHM: allocating %7d bytes in %s (%s: %u)\n", __size, func, file, line);

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return NULL;
     }

     /* Allocate memory from the pool. */
     ret = fusion_shm_pool_allocate( pool, __size + sizeof(SHMemDesc), false, false, &data );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not allocate %d bytes from pool!\n", __size + sizeof(SHMemDesc) );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Fill description. */
     desc = fill_shmem_desc( data, __size, func, file, line );

     /* Add description to list. */
     direct_list_append( &pool->allocs, &desc->link );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     return data + sizeof(SHMemDesc);
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
fusion_dbg_shcalloc( FusionSHMPoolShared *pool,
                     const char *file, int line,
                     const char *func, size_t __nmemb, size_t __size)
{
     DirectResult  ret;
     SHMemDesc    *desc;
     void         *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( __nmemb > 0 );
     D_ASSERT( __size > 0 );

     D_HEAVYDEBUG("Fusion/SHM: allocating %7d bytes in %s (%s: %u)\n", __size, func, file, line);

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return NULL;
     }

     /* Allocate memory from the pool. */
     ret = fusion_shm_pool_allocate( pool, __nmemb * __size + sizeof(SHMemDesc), true, false, &data );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not allocate %d bytes from pool!\n", __nmemb * __size + sizeof(SHMemDesc) );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Fill description. */
     desc = fill_shmem_desc( data, __nmemb * __size, func, file, line );

     /* Add description to list. */
     direct_list_append( &pool->allocs, &desc->link );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     return data + sizeof(SHMemDesc);
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
fusion_dbg_shrealloc( FusionSHMPoolShared *pool,
                      const char *file, int line,
                      const char *func, const char *what, void *__ptr,
                      size_t __size )
{
     DirectResult  ret;
     SHMemDesc    *desc;
     void         *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( what != NULL );

     if (!__ptr)
          return fusion_dbg_shmalloc( pool, file, line, func, __size );

     if (!__size) {
          fusion_dbg_shfree( pool, file, line, func, what, __ptr );
          return NULL;
     }

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return NULL;
     }

     /* Lookup the corresponding description. */
     direct_list_foreach (desc, pool->allocs) {
          if (desc->mem == __ptr)
               break;
     }

     if (!desc) {
          D_ERROR( "Fusion/SHM: Cannot reallocate unknown chunk at %p (%s) from [%s:%d in %s()]!\n",
                   __ptr, what, file, line, func );
          D_BREAK( "unknown chunk" );
          return NULL; /* shouldn't happen due to the break */
     }

     /* Remove the description in case the block moves. */
     direct_list_remove( &pool->allocs, &desc->link );

     /* Reallocate the memory block. */
     ret = fusion_shm_pool_reallocate( pool, __ptr - sizeof(SHMemDesc), __size + sizeof(SHMemDesc), false, &data );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not reallocate from %d to %d bytes!\n",
                    desc->bytes + sizeof(SHMemDesc), __size + sizeof(SHMemDesc) );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Fill description. */
     desc = fill_shmem_desc( data, __size, func, file, line );
     
     /* Add description to list. */
     direct_list_append( &pool->allocs, &desc->link );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     return data + sizeof(SHMemDesc);
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_dbg_shfree( FusionSHMPoolShared *pool,
                   const char *file, int line,
                   const char *func, const char *what, void *__ptr )
{
     DirectResult  ret;
     SHMemDesc    *desc;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( what != NULL );
     D_ASSERT( __ptr != NULL );

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return;
     }

     /* Lookup the corresponding description. */
     direct_list_foreach (desc, pool->allocs) {
          if (desc->mem == __ptr)
               break;
     }

     if (!desc) {
          D_ERROR( "Fusion/SHM: Cannot free unknown chunk at %p (%s) from [%s:%d in %s()]!\n",
                   __ptr, what, file, line, func );
          D_BREAK( "unknown chunk" );
          return; /* shouldn't happen due to the break */
     }

     /* Remove the description. */
     direct_list_remove( &pool->allocs, &desc->link );

     /* Free the memory block. */
     fusion_shm_pool_deallocate( pool, __ptr - sizeof(SHMemDesc), false );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );
}

/* Duplicate string in shared memory. */
char *
fusion_dbg_shstrdup( FusionSHMPoolShared *pool,
                     const char *file, int line,
                     const char *func, const char *string )
{
     DirectResult  ret;
     SHMemDesc    *desc;
     void         *data = NULL;
     int           length;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( string != NULL );

     length = strlen( string ) + 1;

     D_HEAVYDEBUG("Fusion/SHM: allocating %7d bytes in %s (%s: %u)\n", length, func, file, line);

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return NULL;
     }

     /* Allocate memory from the pool. */
     ret = fusion_shm_pool_allocate( pool, length + sizeof(SHMemDesc), false, false, &data );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not allocate %d bytes from pool!\n", length + sizeof(SHMemDesc) );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Fill description. */
     desc = fill_shmem_desc( data, length, func, file, line );

     /* Add description to list. */
     direct_list_append( &pool->allocs, &desc->link );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     /* Copy string content. */
     direct_memcpy( data + sizeof(SHMemDesc), string, length );

     return data + sizeof(SHMemDesc);
}

#else

/* Allocate SIZE bytes of memory.  */
void *
fusion_shmalloc( FusionSHMPoolShared *pool, size_t __size )
{
     void *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( __size > 0 );

     if (fusion_shm_pool_allocate( pool, __size, false, true, &data ))
          return NULL;

     D_ASSERT( data != NULL );

     return data;
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
fusion_shcalloc( FusionSHMPoolShared *pool, size_t __nmemb, size_t __size )
{
     void *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( __nmemb > 0 );
     D_ASSERT( __size > 0 );

     if (fusion_shm_pool_allocate( pool, __nmemb * __size, true, true, &data ))
          return NULL;

     D_ASSERT( data != NULL );

     return data;
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
fusion_shrealloc( FusionSHMPoolShared *pool, void *__ptr, size_t __size )
{
     void *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     if (!__ptr)
          return fusion_shmalloc( pool, __size );

     if (!__size) {
          fusion_shfree( pool, __ptr );
          return NULL;
     }

     if (fusion_shm_pool_reallocate( pool, __ptr, __size, true, &data ))
          return NULL;

     D_ASSERT( data != NULL || __size == 0 );

     return data;
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_shfree( FusionSHMPoolShared *pool, void *__ptr )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( __ptr != NULL );

     fusion_shm_pool_deallocate( pool, __ptr, true );
}

/* Duplicate string in shared memory. */
char *
fusion_shstrdup( FusionSHMPoolShared *pool, const char* string )
{
     int   len;
     void *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( string != NULL );

     len = strlen( string ) + 1;

     if (fusion_shm_pool_allocate( pool, len, false, true, &data ))
          return NULL;

     D_ASSERT( data != NULL );

     direct_memcpy( data, string, len );

     return data;
}

#endif

#else

/************************** SINGLE APPLICATION CORE ***************************/

#if DIRECT_BUILD_DEBUG

#include <direct/mem.h>

/* Allocate SIZE bytes of memory.  */
void *
fusion_dbg_shmalloc( FusionSHMPoolShared *pool,
                     const char *file, int line,
                     const char *func, size_t __size )
{
     D_ASSERT( __size > 0 );

     return direct_malloc( file, line, func, __size );
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
fusion_dbg_shcalloc( FusionSHMPoolShared *pool,
                     const char *file, int line,
                     const char *func, size_t __nmemb, size_t __size)
{
     D_ASSERT( __nmemb > 0 );
     D_ASSERT( __size > 0 );

     return direct_calloc( file, line, func, __nmemb, __size );
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
fusion_dbg_shrealloc( FusionSHMPoolShared *pool,
                      const char *file, int line,
                      const char *func, const char *what, void *__ptr,
                      size_t __size )
{
     return direct_realloc( file, line, func, what, __ptr, __size );
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_dbg_shfree( FusionSHMPoolShared *pool,
                   const char *file, int line,
                   const char *func, const char *what, void *__ptr )
{
     D_ASSERT( __ptr != NULL );

     direct_free( file, line, func, what, __ptr );
}

/* Duplicate string in shared memory. */
char *
fusion_dbg_shstrdup( FusionSHMPoolShared *pool,
                     const char *file, int line,
                     const char *func, const char *string )
{
     D_ASSERT( string != NULL );

     return direct_strdup( file, line, func, string );
}

#else

/* Allocate SIZE bytes of memory.  */
void *
fusion_shmalloc (FusionSHMPoolShared *pool,
                 size_t __size)
{
     D_ASSERT( __size > 0 );

     return malloc( __size );
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
fusion_shcalloc (FusionSHMPoolShared *pool,
                 size_t __nmemb, size_t __size)
{
     D_ASSERT( __nmemb > 0 );
     D_ASSERT( __size > 0 );

     return calloc( __nmemb, __size );
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
fusion_shrealloc (FusionSHMPoolShared *pool,
                  void *__ptr, size_t __size)
{
     return realloc( __ptr, __size );
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_shfree (FusionSHMPoolShared *pool,
               void *__ptr)
{
     D_ASSERT( __ptr != NULL );

     free( __ptr );
}

/* Duplicate string in shared memory. */
char *
fusion_shstrdup (FusionSHMPoolShared *pool,
                 const char          *string)
{
     D_ASSERT( string != NULL );

     return strdup( string );
}

#endif

#endif
