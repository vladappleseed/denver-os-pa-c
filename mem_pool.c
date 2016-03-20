// Created by Ivo Georgiev on 2/9/16.
//------------------------------------------------------------
// Function declarations added by Vladislav Makarov on 3/6/16.
// Last edit was made by Vladislav Makarov on 3/20/16.

#include <stdlib.h>
#include <assert.h>
#include <stdio.h> // for perror()

#include "mem_pool.h"

/*************/
/*           */
/* Constants */
/*           */
/*************/
static const float      MEM_FILL_FACTOR                 = 0.75;
static const unsigned   MEM_EXPAND_FACTOR               = 2;

static const unsigned   MEM_POOL_STORE_INIT_CAPACITY    = 20;
static const float      MEM_POOL_STORE_FILL_FACTOR      = 0.75;
static const unsigned   MEM_POOL_STORE_EXPAND_FACTOR    = 2;

static const unsigned   MEM_NODE_HEAP_INIT_CAPACITY     = 40;
static const float      MEM_NODE_HEAP_FILL_FACTOR       = 0.75;
static const unsigned   MEM_NODE_HEAP_EXPAND_FACTOR     = 2;

static const unsigned   MEM_GAP_IX_INIT_CAPACITY        = 40;
static const float      MEM_GAP_IX_FILL_FACTOR          = 0.75;
static const unsigned   MEM_GAP_IX_EXPAND_FACTOR        = 2;



/*********************/
/*                   */
/* Type declarations */
/*                   */
/*********************/
typedef struct _node {
    alloc_t alloc_record;
    unsigned used;
    unsigned allocated;
    struct _node *next, *prev; // doubly-linked list for gap deletion
} node_t, *node_pt;

typedef struct _gap {
    size_t size;
    node_pt node;
} gap_t, *gap_pt;

typedef struct _pool_mgr {
    pool_t pool;
    node_pt node_heap;
    unsigned total_nodes;
    unsigned used_nodes;
    gap_pt gap_ix;
    unsigned gap_ix_capacity;
} pool_mgr_t, *pool_mgr_pt;



/***************************/
/*                         */
/* Static global variables */
/*                         */
/***************************/
static pool_mgr_pt *pool_store = NULL; // an array of pointers, only expand
static unsigned pool_store_size = 0;
static unsigned pool_store_capacity = 0;



/********************************************/
/*                                          */
/* Forward declarations of static functions */
/*                                          */
/********************************************/
static alloc_status _mem_resize_pool_store();
static alloc_status _mem_resize_node_heap(pool_mgr_pt pool_mgr);
static alloc_status _mem_resize_gap_ix(pool_mgr_pt pool_mgr);
static alloc_status _mem_add_to_gap_ix(pool_mgr_pt pool_mgr, size_t size, node_pt node);
static alloc_status _mem_remove_from_gap_ix(pool_mgr_pt pool_mgr, size_t size, node_pt node);
static alloc_status _mem_sort_gap_ix(pool_mgr_pt pool_mgr);



/****************************************/
/*                                      */
/* Definitions of user-facing functions */
/*                                      */
/****************************************/
alloc_status mem_init()
{
    //-------------------------------------------------------------------
    // Instructor comments
    //-------------------------------------------------------------------
    // ensure that it's called only once until mem_free
    // allocate the pool store with initial capacity
    // note: holds pointers only, other functions to allocate/deallocate
    //-------------------------------------------------------------------

    // if the memory pool store has already been initialized
    if (pool_store != NULL)
    {
        // do a return with the corresponding return agrument representing our allocation status
        return ALLOC_CALLED_AGAIN;
    }

        // if the the memory pool store has NOT YET been initialized
    else if (pool_store == NULL)
    {
        pool_store = (pool_mgr_pt *) calloc(MEM_POOL_STORE_INIT_CAPACITY, sizeof(pool_mgr_t));

        // Now, check whether the allocation above succeeded
        if (pool_store == NULL)                                                     // [1] if it DID NOT succeed, handle it appropriately
        {
            return ALLOC_FAIL;                                                      // do a return with the corresponding return argument
        }

        // [2] if it DID succeed, finish the memory pool store initialization
        pool_store_capacity = MEM_POOL_STORE_INIT_CAPACITY;                         // set pool store capacity to initial capacity
        pool_store_size = 0;                                                        // set pool store size to initial value (zero)

        return ALLOC_OK;                                                            // do a return with the corresponding return argument
    }
}


/*=================================================== alloc_status mem_free function ===================================================*/
alloc_status mem_free()
{
    //------------------------------------------------------
    // Instructor comments
    //------------------------------------------------------
    // ensure that it's called only once for each mem_init
    // make sure all pool managers have been deallocated
    // can free the pool store array
    // update static variables
    //------------------------------------------------------

    // if the memory pool store has not yet been initialized, but mem_free was already called
    if (pool_store == NULL)
    {
        return ALLOC_CALLED_AGAIN;                                             // do a return with the corresponding return argument
    }

        // if the store was already initialized prior to mem_free call, we're good to go
    else if (pool_store != NULL)
    {
        int pool_alloc_status = 0;

        int i;
        for (i = 0; i < pool_store_capacity; i++)                               // go through all the pools in the pool store, make sure all have been deallocated
        {
            if (pool_store[i] != NULL)
            {
                if (!pool_alloc_status)
                {
                    pool_alloc_status = 1;
                }
            }
        }

        if (pool_alloc_status)
        {
            return ALLOC_FAIL;
        }

        // free the pool store and update the static variables
        free(pool_store);
        pool_store = NULL;
        pool_store_capacity = 0;
        pool_store_size = 0;

        return ALLOC_OK;                                                       // do a return with the corresponding return argument
    }
}


/*=================================================== pool_pt mem_pool_open function ===================================================*/
pool_pt mem_pool_open(size_t size, alloc_policy policy)
{
    //------------------------------------------------------------------
    // Instructor comments
    //------------------------------------------------------------------
    // make sure that the pool store is allocated
    // expand the pool store, if necessary
    // allocate a new mem pool mgr
    // check success, on error return null
    // allocate a new memory pool
    // check success, on error deallocate mgr and return null
    // allocate a new node heap
    // check success, on error deallocate mgr/pool and return null
    // allocate a new gap index
    // check success, on error deallocate mgr/pool/heap and return null
    // assign all the pointers and update meta data:
    //   initialize top node of node heap
    //   initialize top node of gap index
    //   initialize pool mgr
    //   link pool mgr to pool store
    // return the address of the mgr, cast to (pool_pt)
    //------------------------------------------------------------------

    // make sure that the pool store is allocated
    // if the pool store HAS NOT been allocated yet
    if (pool_store == NULL)
    {
        alloc_status call_status = mem_init();                                  // allocate it
        if (call_status == ALLOC_FAIL)                                          // if allocation did not successeed, return NULL
        {
            return NULL;
        }
    }

    // if the pool store is already allocated
    if (pool_store != NULL)
    {
        _mem_resize_pool_store();                                               // expand the pool store, if necessary

        pool_mgr_pt new_pool_mgr = calloc(1, sizeof(pool_mgr_t));               // allocate a new mem pool mgr
        if (new_pool_mgr == NULL)                                               // check success, on error return null
        {
            return NULL;
        }

        // allocate a new memory pool
        new_pool_mgr->pool.mem = malloc(size);

        if (new_pool_mgr->pool.mem == NULL)                                     // check success, on error deallocate mgr and return null
        {
            free(new_pool_mgr);                                                 // deallocate the pool mgr
            return NULL;                                                        // return NULL
        }

        // initialize the memory pool
        new_pool_mgr->pool.policy = policy;
        new_pool_mgr->pool.total_size = size;
        new_pool_mgr->pool.alloc_size = 0;
        new_pool_mgr->pool.num_allocs = 0;
        new_pool_mgr->pool.num_gaps = 1;

        // allocate a new node heap
        new_pool_mgr->node_heap = calloc(MEM_NODE_HEAP_INIT_CAPACITY, sizeof(node_t));
        new_pool_mgr->total_nodes = MEM_NODE_HEAP_INIT_CAPACITY;

        if (new_pool_mgr->node_heap == NULL)                                    // if the allocation of the new node heap has failed
        {
            free(new_pool_mgr->pool.mem);                                       // deallocate the memory pool
            free(new_pool_mgr);                                                 // deallocate the pool mgr

            return NULL;                                                        // return NULL
        }

        // allocate a new gap index
        new_pool_mgr->gap_ix = calloc(MEM_GAP_IX_INIT_CAPACITY, sizeof(gap_t));
        new_pool_mgr->gap_ix_capacity = MEM_GAP_IX_INIT_CAPACITY;

        if (new_pool_mgr->gap_ix == NULL)                                       // if the allocation of the new gap index has failed
        {
            free(new_pool_mgr->node_heap);                                      // deallocate the node heap
            free(&new_pool_mgr->pool);                                          // deallocate the memory pool
            free(new_pool_mgr);                                                 // deallocate the pool mgr

            return NULL;                                                        // return NULL
        }

        // assign all the pointers and update meta data:

        // initialize top node of node heap
        new_pool_mgr->node_heap->prev = NULL;
        new_pool_mgr->node_heap->next = NULL;
        new_pool_mgr->node_heap->used = 0;
        new_pool_mgr->node_heap->allocated = 0;
        new_pool_mgr->node_heap->alloc_record.mem = new_pool_mgr->pool.mem;
        new_pool_mgr->node_heap->alloc_record.size = size;

        // initialize top node of gap index
        new_pool_mgr->gap_ix->node = new_pool_mgr->node_heap;
        new_pool_mgr->gap_ix->size = size;

        // initialize pool mgr
        new_pool_mgr->pool.policy = policy;
        new_pool_mgr->pool.total_size = size;
        new_pool_mgr->pool.alloc_size = 0;
        new_pool_mgr->pool.num_allocs = 0;
        new_pool_mgr->pool.num_gaps = 1;
        new_pool_mgr->used_nodes = 1;

        // link pool mgr to pool store
        pool_store[pool_store_size] = new_pool_mgr;

        pool_store_size = 1;                                                    // set pool store size equal to 1

        return (pool_pt) new_pool_mgr;                                          // return the address of the mgr, cast to (pool_pt)
    }
}


/*================================================ alloc_status mem_pool_close function ================================================*/
alloc_status mem_pool_close(pool_pt pool)
{
    //--------------------------------------------------------------
    // Instructor comments
    //--------------------------------------------------------------
    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    // check if this pool is allocated
    // check if pool has only one gap
    // check if it has zero allocations
    // free memory pool
    // free node heap
    // free gap index
    // find mgr in pool store and set to null
    // note: don't decrement pool_store_size, because it only grows
    // free mgr
    //--------------------------------------------------------------

    const pool_mgr_pt new_pool_mgr = (pool_mgr_pt) pool;                                // get mgr from pool by casting the pointer to (pool_mgr_pt)
    if (new_pool_mgr != NULL)                                                           // if this pool is allocated, go on
    {
        if (pool->num_gaps != 1)                                                        // check if the pool has only one gap
        {
            return ALLOC_NOT_FREED;                                                     // if it doesn't, handle it appropriately
        }

        if (pool->num_allocs != 0)                                                      // check if the pool has zero allocations
        {
            return ALLOC_NOT_FREED;                                                     // if it doesn't, handle it appropriately
        }

        free(new_pool_mgr->pool.mem);                                                   // free memory pool
        free(new_pool_mgr->node_heap);                                                  // free node heap
        free(new_pool_mgr->gap_ix);                                                     // free gap index

        // now, find mgr in pool store and set to null
        int i;
        for (i = 0; i < pool_store_size; i++)
        {
            if(pool_store[i] == new_pool_mgr)
            {
                pool_store[i] = NULL;                                                   // instance found -> set it equal to null
                i = pool_store_size;                                                    // we're done, set i to pool_store_size (so we don't loop further)
            }
        }
        free(new_pool_mgr);                                                             // final step: free mgr

        return ALLOC_OK;                                                                // all done with no errors, return ALLOC_OK
    }
    else if (new_pool_mgr == NULL)                                                      // if this pool is not allocated, return ALLOC_NOT_FREED
    {
        return ALLOC_NOT_FREED;
    }
}


/*================================================== alloc_pt mem_new_alloc function ===================================================*/
alloc_pt mem_new_alloc(pool_pt pool, size_t size)
{
    //--------------------------------------------------------------------
    // Instructor comments
    //--------------------------------------------------------------------
    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    // check if any gaps, return null if none
    // expand heap node, if necessary, quit on error
    // check used nodes fewer than total nodes, quit on error
    // get a node for allocation:
    // if FIRST_FIT, then find the first sufficient node in the node heap
    // if BEST_FIT, then find the first sufficient node in the gap index
    // check if node found
    // update metadata (num_allocs, alloc_size)
    // calculate the size of the remaining gap, if any
    // remove node from gap index
    // convert gap_node to an allocation node of given size
    // adjust node heap:
    //   if remaining gap, need a new node
    //   find an unused one in the node heap
    //   make sure one was found
    //   initialize it to a gap node
    //   update metadata (used_nodes)
    //   update linked list (new node right after the node for allocation)
    //   add to gap index
    //   check if successful
    // return allocation record by casting the node to (alloc_pt)
    //--------------------------------------------------------------------

    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    pool_mgr_pt new_pool_mgr = (pool_mgr_pt) pool;

    // check if any gaps, return null if none
    if (new_pool_mgr->gap_ix == NULL)
    {
        return NULL;
    }

    // expand heap node, if necessary, quit on error
    if (new_pool_mgr->used_nodes / new_pool_mgr->total_nodes > MEM_NODE_HEAP_FILL_FACTOR)
    {
        if (_mem_resize_node_heap(new_pool_mgr) != ALLOC_OK)
        {
            return NULL;
        }
    }

    // check used nodes fewer than total nodes, quit on error
    if (new_pool_mgr->total_nodes < new_pool_mgr->used_nodes)
    {
        return NULL;
    }

    // get a node for allocation:
    node_pt new_node;
    int i = 0;

    // if FIRST_FIT, then find the first sufficient node in the node heap
    if(new_pool_mgr->pool.policy == FIRST_FIT)
    {
        node_pt heap = new_pool_mgr->node_heap;

        while(new_pool_mgr->node_heap[i].alloc_record.size < size || (new_pool_mgr->node_heap[i].allocated != 0 && new_pool_mgr->node_heap[i].used !=0))
        {
            if(&new_pool_mgr->node_heap[i] == new_pool_mgr->gap_ix[0].node && new_pool_mgr->node_heap[i].alloc_record.size < size)
            {
                return NULL;
            }

            i += 1;
        }

        if(heap == NULL)
        {
            return NULL;
        }

        new_node = &new_pool_mgr->node_heap[i];
    }

        // if BEST_FIT, then find the first sufficient node in the gap index
    else if(new_pool_mgr->pool.policy == BEST_FIT)
    {
        if (new_pool_mgr->pool.num_gaps > 0) {
            while (i < new_pool_mgr->pool.num_gaps && new_pool_mgr->gap_ix[i+1].size >= size)
            {
                i += 1;
            }
        }

        else
        {
            return NULL;
        }
        new_node = new_pool_mgr->gap_ix[i].node;
    }

    // check if node found
    // if it's not found, handle appropriately
    if(new_node == NULL)
    {
        return NULL;
    }

    // if it was found
    // update metadata (num_allocs, alloc_size)
    new_pool_mgr->pool.alloc_size += size;
    new_pool_mgr->pool.num_allocs += 1;

    // calculate the size of the remaining gap, if any
    size_t remainder = 0;
    if (new_node->alloc_record.size - size > 0)
    {
        remainder = new_node->alloc_record.size - size;
    }

    // remove node from gap index
    _mem_remove_from_gap_ix(new_pool_mgr, size, new_node);

    // convert gap_node to an allocation node of given size
    new_node->allocated = 1;
    new_node->used = 1;
    new_node->alloc_record.size = size;

    // adjust node heap
    if (remainder != 0)
    {
        // if remaining gap, need a new node
        // find an unused one in the node heap
        int i = 0;
        while (new_pool_mgr->node_heap[i].used != 0)
        {
            i += 1;
        }
        node_pt new_gap = &new_pool_mgr->node_heap[i];

        // initialize it to a gap node
        new_gap->used = 1;
        new_gap->allocated = 0;
        new_gap->alloc_record.size = remainder;

        // update metadata (used_nodes)
        new_pool_mgr->used_nodes += 1;

        // update linked list (new node right after the node for allocation)
        new_node->alloc_record.size = size;

        if(new_node->next)
        {
            new_node->next->prev = new_gap;
        }

        new_gap->next = new_node->next;
        new_node->next = new_gap;
        new_gap->prev = new_node;

        //add to gap index
        _mem_add_to_gap_ix(new_pool_mgr, remainder, new_gap);
    }

    // return allocation record by casting the node to (alloc_pt)
    return (alloc_pt) new_node;
}



/*================================================ alloc_status mem_del_alloc function =================================================*/
alloc_status mem_del_alloc(pool_pt pool, alloc_pt alloc)
{
    //----------------------------------------------------------------------
    // Instructor comments
    //----------------------------------------------------------------------
    // get mgr from pool by casting the pointer to (pool_mgr_pt)
    // get node from alloc by casting the pointer to (node_pt)
    // find the node in the node heap
    // this is node-to-delete
    // make sure it's found
    // convert to gap node
    // update metadata (num_allocs, alloc_size)
    // if the next node in the list is also a gap, merge into node-to-delete
    //   remove the next node from gap index
    //   check success
    //   add the size to the node-to-delete
    //   update node as unused
    //   update metadata (used nodes)
    //   update linked list:
    /*
                    if (next->next) {
                        next->next->prev = node_to_del;
                        node_to_del->next = next->next;
                    } else {
                        node_to_del->next = NULL;
                    }
                    next->next = NULL;
                    next->prev = NULL;
     */

    // this merged node-to-delete might need to be added to the gap index
    // but one more thing to check...
    // if the previous node in the list is also a gap, merge into previous!
    //   remove the previous node from gap index
    //   check success
    //   add the size of node-to-delete to the previous
    //   update node-to-delete as unused
    //   update metadata (used_nodes)
    //   update linked list
    /*
                    if (node_to_del->next) {
                        prev->next = node_to_del->next;
                        node_to_del->next->prev = prev;
                    } else {
                        prev->next = NULL;
                    }
                    node_to_del->next = NULL;
                    node_to_del->prev = NULL;
     */
    //   change the node to add to the previous node!
    // add the resulting node to the gap index
    // check success
    //----------------------------------------------------------------------

    pool_mgr_pt new_pool_mgr = (pool_mgr_pt) pool;                                     // get mgr from pool by casting the pointer to (pool_mgr_pt)
    node_pt node = (node_pt) alloc;                                                    // get node from alloc by casting the pointer to (node_pt)

    node_pt to_delete = NULL;

    // find the node in the node heap
    int i;
    for (i = 0; i < new_pool_mgr->total_nodes; ++i)
    {
        if(node == &new_pool_mgr->node_heap[i])
        {
            to_delete = &new_pool_mgr->node_heap[i];
            break;
        }
    }

    // this is node-to-delete
    // make sure it's found
    // if the node is not found, handle it appropriately
    if (to_delete == NULL)
    {
        return ALLOC_FAIL;
    }

    // if it was found
    // update metadata (num_allocs, alloc_size)
    to_delete->allocated = 0;
    new_pool_mgr->pool.num_allocs -= 1;
    new_pool_mgr->pool.alloc_size = new_pool_mgr->pool.alloc_size - to_delete->alloc_record.size;

    // if the next node in the list is also a gap, merge into node-to-delete
    if (to_delete->next != NULL && to_delete->next->allocated == 0)
    {
        node_pt next = to_delete->next;
        if (_mem_remove_from_gap_ix(new_pool_mgr, 0, next) == ALLOC_FAIL)
        {
            return ALLOC_FAIL;
        }

        to_delete->alloc_record.size += next->alloc_record.size;
        next->used = 0;
        new_pool_mgr->used_nodes -= 1;

        if (next->next)
        {
            next->next->prev = to_delete;
            to_delete->next = next->next;
        }

        else
        {
            to_delete->next = NULL;
        }

        next->next = NULL;
        next->prev = NULL;
    }

    // this merged node-to-delete might need to be added to the gap index
    // if the previous node in the list is also a gap, merge into previous!
    if(to_delete->prev != NULL && to_delete->prev->allocated == 0)
    {
        node_pt previous = to_delete->prev;
        if (_mem_remove_from_gap_ix(new_pool_mgr, 0, previous) == ALLOC_FAIL)
        {
            return ALLOC_FAIL;
        }

        previous->alloc_record.size += to_delete->alloc_record.size;
        to_delete->used = 0;
        new_pool_mgr->used_nodes -= 1;
        if(to_delete->next)
        {
            previous->next = to_delete->next;
            to_delete->next->prev = previous;
        }
        else
        {
            previous->next = NULL;
        }

        to_delete = previous;
    }

    // add the resulting node to the gap index
    // check success
    // if no success, handle appropriately
    if (_mem_add_to_gap_ix(new_pool_mgr, to_delete->alloc_record.size, to_delete) != ALLOC_OK)
    {
        return ALLOC_FAIL;
    }
    else
    {
        return ALLOC_OK;
    }
}


/*================================================= (void) mem_inspect_pool function ==================================================*/
void mem_inspect_pool(pool_pt pool, pool_segment_pt *segments, unsigned *num_segments)
{
    //----------------------------------------------------------------
    // Instructor comments
    //----------------------------------------------------------------
    // get the mgr from the pool
    // allocate the segments array with size == used_nodes
    // check successful
    // loop through the node heap and the segments array
    //    for each node, write the size and allocated in the segment
    // "return" the values:
    /*
                    *segments = segs;
                    *num_segments = pool_mgr->used_nodes;
    */
    //----------------------------------------------------------------

    const pool_mgr_pt new_pool_mgr = (pool_mgr_pt) pool;                                     // get mgr from pool by casting the pointer to (pool_mgr_pt)
    const pool_segment_pt segs = malloc(sizeof(pool_segment_t) * new_pool_mgr->used_nodes);  // allocate the segments array with size == used_nodes


    if (segs != NULL)                                                                        // if the allocation was successful, continue
    {
        pool_segment_pt current_seg = segs;
        node_pt current_node = new_pool_mgr->node_heap;

        int i;
        for (i = 0; i < new_pool_mgr->used_nodes; i++)
        {
            current_seg->size = current_node->alloc_record.size;
            current_seg->allocated = current_node->allocated;
            current_seg += 1;
            current_node = current_node->next;
        }

        *segments = segs;
        *num_segments = new_pool_mgr->used_nodes;

        return;                                                                               // "return" the updated values
    }
    else if (segs == NULL)                                                                    // if the allocation was not successful, just do a return
    {
        return;
    }
}




/***********************************/
/*                                 */
/* Definitions of static functions */
/*                                 */
/***********************************/
static alloc_status _mem_resize_pool_store()
{
    //-------------------------------------------------------------
    // Instructor comments
    //-------------------------------------------------------------
    // check if necessary
    /*
    *           if (((float) pool_store_size / pool_store_capacity)
    *               > MEM_POOL_STORE_FILL_FACTOR) {...}
    */
    // don't forget to update capacity variables
    //-------------------------------------------------------------

    if (pool_store_size == pool_store_capacity)                                             // if they are equal, we need to expand our pool store
    {
        pool_store = realloc(pool_store, (pool_store_capacity + MEM_POOL_STORE_INIT_CAPACITY) * sizeof(pool_mgr_pt));
        if (pool_store == NULL)                                                             // check for realloc success, on error return ALLOC_FAIL
        {
            return ALLOC_FAIL;
        }
        pool_store_capacity += MEM_POOL_STORE_INIT_CAPACITY;
    }
    else if (pool_store_size != pool_store_capacity)                                        // if they are not equal, no pool store expansion is needed
    {
        return ALLOC_OK;
    }
}


static alloc_status _mem_resize_node_heap(pool_mgr_pt pool_mgr)
{
    //-------------------------------------------------------------
    // Instructor comments
    //-------------------------------------------------------------
    // check if necessary
    /*
    *           if (((float) pool_store_size / pool_store_capacity)
    *               > MEM_POOL_STORE_FILL_FACTOR) {...}
    */
    // don't forget to update capacity variables
    //-------------------------------------------------------------

    if (((float)pool_mgr->used_nodes / pool_mgr->total_nodes) > MEM_NODE_HEAP_FILL_FACTOR)
    {
        int new_node_count = pool_mgr->total_nodes * MEM_NODE_HEAP_EXPAND_FACTOR;
        int new_heap_size = new_node_count * sizeof(node_t);

        node_pt new_node_heap = (node_pt)malloc(new_heap_size);

        if (new_node_heap != NULL)
        {
            pool_mgr->total_nodes += new_node_count;                                        // update the total number nodes

            // link the chunk of nodes we're adding to the old list
            node_pt last_node = pool_mgr->node_heap;
            node_pt node;
            for (node = pool_mgr->node_heap; node->next != NULL; node = node->next)
            {
                last_node = node;
            }
            last_node->next = new_node_heap;

            // initialize the new nodes and link them together
            node_pt prev_node = NULL;
            node_pt this_node = new_node_heap;

            int i;
            for (i = 0; i < new_node_count; i++)
            {
                this_node->used = 1;
                this_node->allocated = 0;
                this_node->alloc_record.size = 0;
                this_node->alloc_record.mem = 0;
                if (prev_node != NULL)
                {
                    prev_node->next = this_node;
                    this_node->prev = prev_node;
                }
                prev_node = this_node;
                this_node += 1;
            }
        }
        else
        {
            return ALLOC_FAIL;
        }
    }
    return ALLOC_OK;
}


static alloc_status _mem_resize_gap_ix(pool_mgr_pt pool_mgr)
{
    //-------------------------------------------------------------
    // Instructor comments
    //-------------------------------------------------------------
    // check if necessary
    /*
    *           if (((float) pool_store_size / pool_store_capacity)
    *               > MEM_POOL_STORE_FILL_FACTOR) {...}
    */
    // don't forget to update capacity variables
    //-------------------------------------------------------------

    if (pool_mgr->pool.num_gaps / pool_mgr->gap_ix_capacity > MEM_GAP_IX_FILL_FACTOR)
    {
        // reallocate/resize gap index
        pool_mgr->gap_ix = realloc(pool_mgr->gap_ix, sizeof(pool_mgr->gap_ix) * MEM_GAP_IX_EXPAND_FACTOR);

        // update the gap capacity
        pool_mgr->gap_ix_capacity = pool_mgr->gap_ix_capacity * MEM_GAP_IX_EXPAND_FACTOR;

        return ALLOC_OK;
    }
    else
    {
        return ALLOC_FAIL;
    }
}


static alloc_status _mem_add_to_gap_ix(pool_mgr_pt pool_mgr, size_t size,node_pt node)
{
    //-------------------------------------------------------
    // Instructor comments
    //-------------------------------------------------------
    // expand the gap index, if necessary (call the function)
    // add the entry at the end
    // update metadata (num_gaps)
    // sort the gap index (call the function)
    // check success
    //-------------------------------------------------------

    // expand the gap index, if necessary (call the function)
    _mem_resize_gap_ix(pool_mgr);

    // add the entry at the end
    int i = 0;
    while (pool_mgr->gap_ix[i].node != NULL)
    {
        i += 1;
    }

    pool_mgr->gap_ix[i].node = node;
    pool_mgr->gap_ix[i].size = size;

    // update number of gaps
    pool_mgr->pool.num_gaps++;

    // sort the gap index
    if (pool_mgr->gap_ix[i].node != NULL)
    {
        _mem_sort_gap_ix(pool_mgr);
        return ALLOC_OK;
    }
    else
    {
        return ALLOC_FAIL;
    }
}


static alloc_status _mem_remove_from_gap_ix(pool_mgr_pt pool_mgr, size_t size, node_pt node)
{
    //-------------------------------------------------------
    // Instructor comments
    //-------------------------------------------------------
    // find the position of the node in the gap index
    // loop from there to the end of the array:
    //    pull the entries (i.e. copy over) one position up
    //    this effectively deletes the chosen node
    // update metadata (num_gaps)
    // zero out the element at position num_gaps!
    //-------------------------------------------------------

    int i = 0;
    if (i == 0)
    {
        while (pool_mgr->gap_ix[i].node != node)
        {
            i += 1;
        }
    }

    pool_mgr->gap_ix[i].size = 0;
    pool_mgr->gap_ix[i].node = NULL;
    pool_mgr->pool.num_gaps--;

    return ALLOC_OK;
}


// note: only called by _mem_add_to_gap_ix, which appends a single entry
static alloc_status _mem_sort_gap_ix(pool_mgr_pt pool_mgr)
{
    //----------------------------------------------------------------------
    // Instructor comments
    //----------------------------------------------------------------------
    // the new entry is at the end, so "bubble it up"
    // loop from num_gaps - 1 until but not including 0:
    //    if the size of the current entry is less than the previous (u - 1)
    //       swap them (by copying) (remember to use a temporary variable)
    //----------------------------------------------------------------------

    int i = pool_mgr->pool.num_gaps - 1;

    for (i; i >= 0; i--)
    {
        if (pool_mgr->gap_ix[i].size < pool_mgr->gap_ix[i+1].size)
        {
            gap_t temp = pool_mgr->gap_ix[i];
            pool_mgr->gap_ix[i] = pool_mgr->gap_ix[i + 1];
            pool_mgr->gap_ix[i + 1] = temp;
        }
    }

    return ALLOC_OK;
}