#include "globus_i_xio.h"

/*
 *  note:
 *
 *  both globus_xio_driver_server_t and globus_xio_driver_accept_request_t
 *  are internally cast to the same object.  The typedef difference is
 *  there to force an api.  This will help to force the driver to finish
 *  only when the callback returns.
 *
 *  Cancel Process
 *  --------------
 *  The only exception to when the accept can finish before the callback occurs
 *  is when a cancel happens.  In the case the driver should stop what it is
 *  doing and finish with a canceled error.  All drivers above it will get
 *  this error and should finish in the same manner, by cleaning up resources
 *  involved with this accpet and the calling finish with the cancel error.
 *  Once the error reaches the top xio will find all drivers were not notified
 *  of the cancel and ask them to destroy their targets.
 *
 *  Errors
 *  ------
 *  There are two times when an error can happens.  The first is in the 
 *  callback.  The process for dealing with this is exctly as a canceled
 *  accept...  The top callback comes back with an error and all successfully
 *  created driver targets are destroyed.
 *
 *  The second case is if the Pass fails.  Once a driver calls pass it must
 *  only return the error code that pass returns.  If that error code is 
 *  GLOBUS_SUCESS then the driver should expect a callback, if it is not 
 *  the driver will not receive the callback for which it registered.  This
 *  rule allows the framework to know that if it receives an error from
 *  pass at the top level that no driver has an outstanding callback.
 *  
 */
#define GlobusXIODriverPassServerAccept(res, op, cb, user_arg)              \
do                                                                          \
{                                                                           \
    globus_i_xio_op_t *                             _op;                    \
    globus_i_xio_server_t *                         _server;                \
    globus_i_xio_server_entry_t *                   _next_entry;            \
    globus_i_xio_server_entry_t *                   _my_entry;              \
                                                                            \
    _op = (globus_i_xio_op_t *)(op);                                        \
    _server = _op->server;                                                  \
                                                                            \
    globus_assert(_op->ndx < _server->stack_size);                          \
    if(_op->canceled)                                                       \
    {                                                                       \
        out_res = OperationHasBeenCacneled();                               \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        _op->progress = GLOBUS_TRUE;                                        \
        _my_entry = &_server->entry[_op->ndx];                              \
        _my_entry->cb = (cb);                                               \
        _my_entry->user_arg = (user_arg);                                   \
        _my_entry->in_register = GLOBUS_TRUE;                               \
        do                                                                  \
        {                                                                   \
            _op->ndx++;                                                     \
            _next_entry = &_server->entry[_op->ndx];                        \
        }                                                                   \
        while(_next_entry->driver->server_accept_func == NULL)              \
                                                                            \
        /* at time that stack is built this will be varified */             \
        globus_assert(_server->ndx <= _server->stack_size);                 \
        res = _next_entry->driver->server_accept_func(                      \
                    _next_entry->server_handle,                             \
                    _next_entry->server_attr, /* TODO:THIS ACCESS IS WORNG*/\
                    _op);                                                   \
        _my_entry->in_register = GLOBUS_FALSE;                              \
    }                                                                       \
}

#define GlobusXIODriverFinishedAccept(server, target, result)               \
do                                                                          \
{                                                                           \
    globus_i_xio_op_t *                             _op;                    \
    globus_i_xio_server_entry_t *                   _entry;                 \
    globus_i_xio_server_t *                         _server;                \
                                                                            \
    _op = (globus_i_xio_op_t *)(op);                                        \
    _server = _op->server;                                                  \
    globus_assert(_server->ndx > 0);                                        \
    _op = GLOBUS_TRUE;                                                      \
                                                                            \
    _op->entry[_op->ndx].target = (target);                                 \
                                                                            \
    do                                                                      \
    {                                                                       \
        _op->ndx--;                                                         \
    }                                                                       \
    while(_server->entry[_op->ndx].cb == NULL &&                            \
            _op->ndx != 0)                                                  \
                                                                            \
    if(_server->entry[_op->ndx].in_register)                                \
    {                                                                       \
        _server->cached_res = (result);                                     \
        globus_callback_space_register_oneshot(                             \
            NULL,                                                           \
            NULL,                                                           \
            globus_l_xio_serveraccept_kickout,                              \
            (void *)_server,                                                \
            GLOBUS_CALLBACK_GLOBAL_SPACE);                                  \
    }                                                                       \
    else                                                                    \
    {                                                                       \
        _server->entry[_op->ndx].cb(_server, result,                        \
            _server->entry[_op->ndx].user_arg);                             \
    }                                                                       \
} while(0)

#define GlobusXIOServerEnableCancel(server, canceled, cb, user_arg)         \
do                                                                          \
{                                                                           \
    globus_i_xio_server_t *                         _server;                \
                                                                            \
    _server = (globus_i_xio_server_t *)(server);                            \
    globus_mutex_lock(&_server->mutex);                                     \
    {                                                                       \
        canceled = _server->canceled;                                       \
        if(!canceled)                                                       \
        {                                                                   \
            _server->cancel_cb = (cb);                                      \
            _server->cancel_user_arg = (user_arg);                          \
        }                                                                   \
    }                                                                       \
    globus_mutex_unlock(&_server->mutex);                                   \
} while(0)

#define GlobusXIOServerDisableCancel(server)                                \
do                                                                          \
{                                                                           \
    globus_i_xio_server_t *                         _server;                \
                                                                            \
    _server = (globus_i_xio_server_t *)(server);                            \
    globus_mutex_lock(&_server->mutex);                                     \
    {                                                                       \
        _server->cancel_cb = NULL;                                          \
        _server->cancel_user_arg = NULL;                                    \
    }                                                                       \
    globus_mutex_unlock(&_server->mutex);                                   \
} while(0)

#define GlobusIXIOTargetDriverDestroy(res, op)                              \
do                                                                          \
{                                                                           \
    int                                                 _ctr;               \
    globus_i_xio_op_t *                                 _op;                \
    globus_result_t                                     tmp_res;            \
                                                                            \
    res = GLOBUS_SUCCESS;                                                   \
    _op = (op);                                                             \
    for(_ctr = 0; _op->stack_size; _ctr++)                                  \
    {                                                                       \
        /* if a target was intialized we need to destroy it */              \
        if(_op->entry[ctr].target != NULL)                                  \
        {                                                                   \
            tmp_res = _op->entry[ctr].driver.target_destroy_func(           \
                _op->entry[ctr].target);                                    \
            if(tmp_res != GLOBUS_SUCCESS)                                   \
            {                                                               \
                res = tmp_res;                                              \
            }                                                               \
        }                                                                   \
    }                                                                       \
} while(0)

/**************************************************************************
 *                       Internal functions
 *                       ------------------
 *************************************************************************/
/*
 *  driver callback kickout
 *
 *  when in a register the finish function kicks this out as a oneshot
 */
void
globus_l_xio_server_driver_accept_kickout(
    void *                                      user_arg)
{
    globus_i_xio_server_t *                     xio_server;

    xio_server = (globus_i_xio_server_t *) user_arg;

    xio_server->entry[xio_server->ndx].cb(
        xio_server, 
        xio_server->cached_res,
        xio_server->entry[xio_server->ndx].user-arg);
}

/*
 *  this is the only mechanism for delivering a callback to the user 
 */
void
globus_l_xio_server_accept_kickout(
    globus_i_xio_op_t *                         xio_op)
{
    /* call the users callback */

    xio_op->accept_cb(
        xio_op,
        xio_op->xio_server,
        xio_op->cached_res,
        xio_op->accept_arg);
 
    /* lock up and do some clean up */
 
    globus_mutex_lock(&xio_server->mutex);
    {
        globus_assert(xio_server->state == GLOBUS_XIO_SERVER_STATE_ACCEPTING);
        globus_assert(xio_op->state == GLOBUS_XIO_TARGET_STATE_SERVER ||
                xio_op->state == GLOBUS_XIO_TARGET_STATE_CLOSED);

        /* decrement reference for the callback */
        xio_op->ref--;
        /* the only way this can be zero is if destroy was called while 
            unlocked for the callback. and the user can only do that if
            result was successful and state was SERVER */
        if(xio_op->ref == 0)
        {
            globus_assert(xio_op->cached_res == GLOBUS_SUCCESS);
            xio_op->ref--;
            globus_free(xio_op);
            if(xio_server->ref == 0)
            {
                assert(xio_server->state == GLOBUS_XIO_SERVER_STATE_CLOSED);
                globus_free(xio_server);
            }
        }
        /* if the operation failed we move target to closed state */
        else if(xio_op->cached_res != GLOBUS_SUCCESS)
        {
            /* insit if error we are in the closed state */
            globus_assert(xio_op->state == GLOBUS_XIO_TARGET_STATE_CLOSED);
            /* If an error occured we walk through all the drivers that
               created finished successfully and free tell them to 
               destroy their targets. If the driver did not initialize a
               target its target entry will be null.  It should be the case
               that only drivers lower in the stack than the one that 
               reported the error have targets but we make no check for
               this in case the driver stack did something strange */
            GlobusIXIOTargetDriverDestroy(res, xio_op);

            xio_op->ref--;
            if(xio_op->ref == 0)
            {
                xio_server->ref--;
                globus_free(xio_op);
                if(xio_server->ref == 0)
                {
                    globus_assert(
                        xio_server->state == GLOBUS_XIO_SERVER_STATE_CLOSED);
                    globus_free(xio_server);
                }
            }
        }
    }
    globus_mutex_unlock(&xio_server->mutex);
}

/*
 *  internal top level accept callback
 */
void
globus_i_xio_server_accept_callback(
    globus_xio_driver_server_t                  server_handle,
    globus_result_t                             result,
    void *                                      user_arg)
{
    globus_i_xio_server_t *                     xio_server;
    globus_i_xio_op_entry_t *                   xio_op;
    globus_bool_t                               accept = GLOBUS_TRUE;

    xio_server = server;
    xio_op = xio_server->op;

    globus_mutex_lock(&xio_server->mutex);
    {
        /* if in this state it means that the user either has or is about to
           get a cancel callback.  we must delay the delivery of this
           callback until that returns */
        xio_op->cached_res = result;
        if(xio_op->state == GLOBUS_XIO_TARGET_STATE_TIMEOUT_PENDING)
        {
            accept = GLOBUS_FALSE;
            xio_op->state = GLOBUS_XIO_TARGET_STATE_ACCEPT_WAITING;
        }
        else
        {
            /* if there is an outstanding accept callback */
            if(xio_op->accept_timeout != NULL)
            {
                if(globus_i_xio_timer_unregister_timeout(xio_op))
                {
                    xio_op->ref--;
                    globus_assert(xio_op->ref > 0);
                }
            }

            if(xio_op->canceled)
            {
                xio_op->cached_res = GlobusXIOServerCanceled();
            }

            if(xio_op->cached_res == GLOBUS_SUCCESS)
            {
                xio_op->state = GLOBUS_XIO_TARGET_STATE_ACCEPT_SERVER;
            }
            else
            {
                xio_op->state = GLOBUS_XIO_TARGET_STATE_ACCEPT_CLOSED;
            }
        }
    }
    globus_mutex_unlock(&xio_server->mutex);

    /* we may be delaying the callback until cancel returns */
    if(accept)
    {
        globus_l_xio_server_accept_kickout(xio_op);
    }
}

globus_bool_t
globus_l_xio_accept_timeout_callback(
    void *                                      user_arg)
{
    globus_i_xio_op_t *                         xio_op;
    globus_i_xio_server_t *                     xio_server;
    globus_bool_t                               rc;
    globus_bool_t                               accept;
    globus_bool_t                               timeout;

    xio_op = (globus_i_xio_op_t *) user_arg;
    xio_server = xio_op->xio_server;

    globus_mutex_lock(&xio_server->mutex);
    {
        switch(xio_op->state)
        {
            /* this case only happens when we have a timeout registered on the
            target taht we were unable to cancel and the AcceptPass failed */
            case GLOBUS_XIO_TARGET_STATE_CLOSED:

                /* insist that only 1 ref is left for this target, don't 
                   bother decrementing, just destroy it */
                assert(xio_op->ref == 1);
                globus_free(xio_op);
                /* remove the reference for the target on the server */
                xio_server->ref--;
                if(xio_server->ref == 0)
                {
                    /* if reference count is 0 we must be in CLOSED state */
                    globus_assert(
                        xio_server->state == GLOBUS_XIO_SERVER_STATE_CLOSED);
                    globus_free(xio_server);
                }

                /* remove it from the timeout list */
                rc = GLOBUS_TRUE;
                break;

            /* this case happens when the server was succefully created but
                we were unable to cancel the timeout callback */
            case GLOBUS_XIO_TARGET_STATE_SERVER:

                /* decerement the reference for the timeout callback */
                xio_op->ref--;

                /* target should never hit zero in this state */
                globus_assert(xio_op->ref > 0);
                /* remove it from the timeout list */
                rc = GLOBUS_TRUE;
                break;

            /* this case happens when we actually want to cancel the operation
                The timeout code should insure that prograess is false if this
                gets called in this state */
            case GLOBUS_XIO_TARGET_STATE_ACCEPTING:
                /* it is up to the timeout callback to set this to true */
                rc = GLOBUS_FALSE;
                /* cancel the sucker */
                globus_assert(!xio_op->progress);
                globus_assert(xio_op->timeout_cb != NULL);

                /* we don't need to cache the server object in local stack
                   because state insures that it will not go away */

                /* put in canceling state to delay the accept callback */
                xio_op->state = GLOBUS_XIO_TARGET_TYPE_TIMEOUT_PENDING;
                break;

            /* no reason we should get to this state */
            case GLOBUS_XIO_TARGET_STATE_CLIENT:
            default:
                globus_assert(0);
        }
    }
    globus_mutex_unlock(&xio_server->mutex);

    /* if in cancel state, verfiy with user that they want to cancel */
    if(timeout)
    {
        cancel = xio_op->timeout_cb(xio_server);
    }
    /* all non time out casses can just return */
    else
    {
        return rc;
    }

    globus_mutex_lock(&xio_server->mutex);
    {
        /* if canceling set the res and we will remove this timer event */
        if(cancel)
        {
            xio_op->cached_res = GlouxXIOErrorCanceled();
            rc = GLOBUS_TRUE;
            xio_op->canceled = GLOBUS_TRUE;
            if(xio_op->cancel_cb)
            {
                xio_op->cancel_cb(xio_op);
            }            
        }

        /* if an accept callback has already arriverd set flag to later
            call accept callback and set rc to remove timed event */
        if(xio_op->state == GLOBUS_XIO_TARGET_STATE_ACCEPT_WAITING)
        {
            accept = GLOBUS_TRUE;
            rc = GLOBUS_TRUE;

            /* set the rext state based on the result code */
            if(xio_op->cached_res == GLOBUS_SUCCESS)
            {
                xio_op->state = GLOBUS_XIO_TARGET_STATE_SERVER;
            }
            else
            {
                xio_op->state = GLOBUS_XIO_TARGET_STATE_CLOSED;
            }
        }
        /* if no accept is waiting, set state back to accepting */
        else
        {
            xio_op->state = GLOBUS_XIO_TARGET_STATE_ACCEPTING;
        }

        /* if we are remvoing the timed event */
        if(rc)
        {
            /* decremenet the target reference count and insist that it is
               not zero yet */
            xio_op->ref--;
            globus_assert(xio_op->ref > 0);
        }
    }
    globus_mutex_unlock(&xio_server->mutex);

    /* if the accpet was pending we must call it */
    if(accept)
    {
        globus_l_xio_server_accept_kickout(xio_op);
    }

    return rc;
}

/**************************************************************************
 *                         API functions
 *                         -------------
 *************************************************************************/

/*
 *  function wrappers for the macros.  no real reason to have these
 */
globus_result_t
globus_xio_driver_pass_accept(
    globus_xio_driver_operation_t               accept_op,
    globus_xio_driver_callback_t                cb,
    void *                                      user_arg)
{
    globus_result_t                             res;

    if(server_handle == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_server_init");
    }

    GlobusXIODriverPassServerAccept(res, accept_op, cb, user_arg);

    return res;
}

void
globus_xio_driver_finished_accept(
    globus_xio_driver_operation_t               accept_op,
    void *                                      driver_target,
    globus_result_t                             result)
{
    if(accepted_handle == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_server_init");
    }

    GlobusXIODriverFinishedAccept(accept_op, driver_target, result);
}

void
globus_xio_server_enable_cancel(
    globus_xio_driver_operation_t               accept_op,
    globus_bool_t *                             cancel_now,
    globus_xio_driver_accept_cancel_callback_t  cancel_cb,
    void *                                      user_arg)
{
    GlobusXIOServerEnableCancel(accept_op, *cancel_now, cancel_cb, user_arg);
}

void
globus_xio_server_disable_cancel(
    globus_xio_driver_operation_t               accept_op)
{
    GlobusXIOServerDisableCancel(accept_op);
}


/*
 *  initialize a server structure
 */
globus_result_t
globus_xio_server_init(
    globus_xio_server_t *                       server,
    globus_xio_attr_t                           server_attr,
    globus_xio_stack_t                          stack)
{
    globus_i_xio_server_t *                     xio_server;
    globus_result_t                             res;
    globus_bool_t                               done = GLOBUS_FALSE;
    int                                         ctr;
    int                                         ctr2;
    void *                                      ds_attr = NULL;

    if(server == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_server_init");
    }
    if(stack == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_server_init");
    }
    if(globus_list_empty(stack->driver_stack))
    {
        return GlobusXIOErrorBadParameter("globus_xio_server_init");
    }

    /* take what the user stack has at the time of registration */
    globus_mutex_lock(&stack->mutex);
    {
        stack_size = globus_list_size(stack->driver_stack);
        xio_server = (globus_i_xio_server_t *)
                    globus_malloc(sizeof(globus_i_xio_server_t) +
                            (sizeof(globus_i_xio_server_entry_t) *
                                    stack_size - 1));
        xio_server->stack_size = globus_list_size(stack->driver_stack);
        xio_server->ref = 1;
        xio_server->state = GLOBUS_XIO_SERVER_STATE_OPEN;

        /* timeout handling */
        xio_server->accept_timeout = server_attr->accept_timeout;

        /* walk through the stack and add each entry to the array */
        ctr = 0;
        for(list = stack->driver_stack;
            !globus_list_empty(list) && !done;
            list = globus_list_rest(list))
        {
            xio_server->entry[ctr].driver = (globus_i_xio_driver_t *)
                globus_list_first(list);

            /* no sense bothering if attr is NULL */
            if(server_attr != NULL)
            {
                ds_attr = globus_i_xio_attr_get_ds(
                                server_attr,
                                xio_server->entry[ctr].driver);
            }
            /* call the driver init function */
            res = xio_server->entry[ctr].driver->server_init_func(
                    &xio_server->entry[ctr].server_handle,
                    ds_attr);
            if(res != GLOBUS_SUCCESS)
            {
                /* clean up all the initialized servers */
                for(ctr2 = 0 ctr2 < ctr; ctr2++)
                {
                    xio_server->entry[ctr].driver->server_destroy_func(
                        xio_server->entry[ctr].server_handle);
                }
                done = GLOBUS_TRUE;
            }

            ctr++;
        }
    }
    globus_mutex_unlock(&stack->lock);

    return res;
}

/*
 *
 */
globus_result_t
globus_xio_server_cntl(
    globus_xio_server_t                         server,
    globus_xio_driver_t                         driver,
    int                                         cmd,
    ...)
{
    globus_bool_t                               found = GLOBUS_FALSE;
    int                                         ctr;
    globus_result_t                             res = GLOBUS_SUCCESS;
    va_list                                     ap;

    if(server == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_server_cntl");
    }

    xio_server = server;

    globus_mutex_lock(&stack->mutex);
    {
        if(driver == NULL)
        {
            /* do general things */
        }
        else
        {
            for(ctr = 0; 
                !found && ctr < server->stack_size; 
                ctr++)
            {
                if(server->entry[ctr].driver == driver)
                {
                    found = GLOBUS_TRUE;

                    va_start(ap, cmd);
                    res = server->entry[ctr].driver->server_cntl_func(
                            server->entry[ctr].server_handle,
                            cmd,
                            ap);
                    va_end(ap);
                }
            }
            if(!found)
            {
                res = DriverNotFOund();
            }
        }
    }
    globus_mutex_unlock(&stack->lock);

    return res;
}

/*
 *  register an accept
 */
globus_result_t
globus_xio_server_register_accept(
    globus_xio_server_t                         server,
    globus_xio_attr_t                           accept_attr,
    globus_xio_callback_t                       cb,
    void *                                      user_arg)
{
    globus_result_t                             res = GLOBUS_SUCCESS;
    globus_i_xio_server_t *                     xio_server;
    globus_i_xio_op_entry_t *                   xio_op;

    if(server == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_server_cntl");
    }
    
    xio_server = (globus_i_xio_server_t *) server;

    globus_mutex_lock(&xio_server->mutex);
    {
        if(xio_server->state != GLOBUS_XIO_SERVER_STATE_OPEN)
        {
            res = WrongStateForThis("globus_xio_server_register_accept");
        }
        else
        {
            xio_server->state = GLOBUS_XIO_SERVER_STATE_ACCEPTING;

            xio_op = (globus_i_xio_op_t *)
                            globus_malloc(sizeof(globus_i_xio_op_t) +
                                (sizeof(globus_i_xio_op_entry_t) * 
                                    (xio_server->stack_size - 1)));

            if(xio_op == NULL)
            {
                res = MallocError();
            }
            else
            {
                xio_op->op_type = GLOBUS_XIO_OPERATION_TYPE_TARGET;
                xio_op->state = GLOBUS_XIO_TARGET_STATE_ACCEPTING;
                xio_op->server = xio_server;
                xio_op->ref = 1;
                xio_op->cancel_cb = NULL;
                xio_op->canceled = GLOBUS_FALSE;
                xio_op->timeout_cb = xio_server->timeout_cb;
                xio_op->progess = GLOBUS_TRUE;
                xio_op->ndx = 0;
                xio_op->stack_size = xio_server->stack_size;

                /* get all the driver specific attrs and put htem in the 
                    correct place */
                for(ctr = 0; ctr < xio_op->stack_size; ctr++)
                {
                    xio_op->entry[ctr].accept_attr = 
                        globus_i_xio_attr_get_ds(
                            accept_attr,
                            xio_server->entry[ctr].driver);
                }

                /*i deal with timeout if there is one */
                if(xio_op->timeout_cb != NULL)
                {
                    xio_op->ref++;
                    globus_i_xio_timer_register_timeout(
                        g_globus_l_xio_timeout_timer,
                        xio_op,
                        &xio_op->progess,
                        globus_l_xio_accept_timeout_callback,
                        &l_handle->open_timeout_period);
                }

                /* add a reference to the server for this target */
                xio_server->ref++;
                /* no sense unlocking here since accepts are serialized 
                    anyway */
                GlobusXIODriverPassServerAccept(res, xio_op, \
                    globus_i_xio_server_accept_callback, NULL);

                /* if the register failed */
                if(res != GLOBUS_SUCCESS)
                {
                    /* set target to invalid type */
                    xio_op->state = GLOBUS_XIO_TARGET_TYPE_CLOSED;

                    /* if a timeout was registered we must unregister it */
                    if(xio_op->timeout_cb != NULL)
                    {
                        if(globus_i_xio_timer_unregister_timeout(xio_op))
                        {
                            xio_op->ref--;
                            globus_assert(xio_op->ref > 0);
                        }
                    }
                    xio_op->ref--;
                    if(xio_op == 0)
                    {
                        xio_server->ref--;  /* remove the targets reference */
                        globus_free(xio_op);
                    }
                }
            }
        }
    }
    globus_mutex_unlock(&xio_server->mutex);

    return res;
}

/*
 *  cancel the server
 */
globus_result_t
globus_xio_server_cancel_accept(
    globus_xio_server_t                         server)
{
    globus_result_t                             res = GLOBUS_SUCCESS;
    globus_i_xio_server_t *                     xio_server;

    xio_server = (globus_i_xio_server_t *) server;

    globus_mutex_lock(&xio_server->mutex);
    {
        if(xio_server->state != GLOBUS_XIO_SERVER_STATE_ACCEPTING)
        {
            res = ThereIsNothingToCancel("globus_xio_server_cancel_accept");
        }
        else if(xio_server->canceled)
        {
            res = AlreadyCacneled("globus_xio_server_cancel_accept");
        }
        else
        {
            /* the callback is called locked.  within it the driver is
                allowed limited functionality.  by calling this locked
                can more efficiently pass the operation down the stack */
            xio_server->xio_op->canceled = GLOBUS_TRUE;
            if(xio_server->xio_op->cancel_cb)
            {
                xio_server->xio_op->cancel_cb(xio_server);
            }            
        }
    }
    globus_mutex_unlock(&xio_server->mutex);

    return res;
}

/*
 *  destroy the server
 */
globus_result_t
globus_xio_server_destroy(
    globus_xio_server_t                         server)
{
    globus_i_xio_server_t *                     xio_server;
    globus_result_t                             res = GLOBUS_SUCCESS;
    globus_result_t                             tmp_res;

    if(server == NULL)
    {
        return BadParameter();
    }

    xio_server = (globus_i_xio_server_t *) server;

    globus_mutex_lock(&xio_server->mutex);
    {
        if(xio_server->state == GLOBUS_XIO_SERVER_STATE_ACCEPTING)
        {
            res = NotInCorrectStateError();
        }
        else
        {
            xio_server->state = GLOBUS_XIO_SERVER_STATE_CLOSED;
            for(ctr = 0; ctr < xio_server->stack_size; ctr++)
            {
                /* possible to lose a driver res, but you know.. so what? */
                tmp_res = xio_server->entry[ctr].driver(
                        xio_server->entry[ctr].server_handle);
                if(tmp_res != GLOBUS_SUCCESS)
                {
                    res = ADriverFailedToCompletelyDestroy();
                }
            }

            /* dec refrence count then free.  this makes sure we don't free
               while in a user callback */
            xio_server->ref--;
            if(xio_server->ref == 0)
            {
                globus_free(xio_server);
            }
        }
    }
    globus_mutex_unlock(&xio_server->mutex);

    return res;
}


globus_result_t
globus_xio_target_destroy(
    globus_xio_target_t                         target)
{   
    globus_i_xio_op_t *                         xio_op;
    globus_i_xio_server_t *                     xio_server;

    /*
     *  parameter checking 
     */
    if(target == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_target_cntl");
    }
    if(target->state != GLOBUS_XIO_TARGET_STATE_SERVER &&
        arget->state != GLOBUS_XIO_TARGET_STATE_CLIENT)
    {
        return GlobusXIOErrorBadState("globus_xio_target_cntl");
    }

    xio_op = (globus_i_xio_op_t *) target;
    server = xio_op->server;

    /* This should be safe because the user doesn't see the target until
       it is set into the server stat and the mutex is unlocked.  Once
       the mutex is unlocked in the SERVER state it is not chacneged until
       this function is called */
    if(xio_op->state == GLOBUS_XIO_TARGET_STATE_SERVER)
    {
        /* in this state we must mind the reference count */
        globus_mutex_lock(&xio_server->mutex);
        {
            xio_op->state = GLOBUS_XIO_TARGET_STATE_CLOSED;
            xio_xio_op->ref--;
            if(xio_xio_op->ref == 0)
            {
                GlobusIXIOTargetDriverDestroy(res, xio_op);
                globus_free(xio_op);
                xio_server->ref--;
                if(xio_server->ref == 0)
                {
                    assert(xio_server->state == GLOBUS_XIO_SERVER_STATE_CLOSED);
                    globus_free(xio_server);
                }
            }
        }
        globus_mutex_unlock(&xio_server->mutex);
    }
    else if(xio_op->state == GLOBUS_XIO_TARGET_STATE_CLIENT)
    {
        GlobusIXIOTargetDriverDestroy(res, xio_op);
        globus_free(xio_op);
    }

    return res;
}

/*
 *  verify the driver is in this stack.
 *  call target control on the driver
 *
 *  if not driver specific there is nothing to do (yet)
 */
globus_result_t
globus_xio_target_cntl(
    globus_xio_target_t *                       target,
    globus_xio_driver_t *                       driver,
    int                                         cmd,
    ...)
{
    globus_i_xio_op_t *                         xio_op;
    int                                         ctr;
    globus_result_t                             res;
    va_list                                     ap;

    if(target == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_target_cntl");
    }
    if(cmd < 0)
    {
        return GlobusXIOErrorBadParameter("globus_xio_target_cntl");
    }
    
#   ifdef HAVE_STDARG_H
    {
        va_start(ap, cmd);
    }
#   else
    {
        va_start(ap);
    }
#   endif

    /* TODO: does this need a mutex */
    res = globus_i_xio_target_cntl(l_target, l_driver, cmd, ap);

    xio_op = (globus_i_xio_op_t *) target;

    if(driver != NULL)
    {
        for(ctr = 0; ctr < l_target->stack_size; ctr++)
        {
            if(xio_op->entry[ctr].driver == driver)
            {
                GlobusXIODriverTargetCntl(
                    res,
                    driver,
                    xio_op->entry[ctr].target,
                    cmd,
                    ap);

                return res;
            }
        }
        return GlobusXIOErrorDriverNotFound("globus_i_xio_target_cntl");
    }
    else
    {
        /* do general target modifications */
    }

    va_end(ap);

    return GLOBUS_SUCCESS;
}

globus_result_t
globus_xio_target_init(
    globus_xio_target_t *                       target,
    globus_xio_attr_t                           target_attr,
    const char *                                contact_string,
    globus_xio_stack_t                          stack)
{
    struct globus_i_xio_target_s *              l_target;

    /*
     *  parameter checking 
     */
    if(target == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_target_init");
    }
    if(contact_string == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_target_init");
    }
    if(stack == NULL)
    {
        return GlobusXIOErrorBadParameter("globus_xio_target_init");
    }

    l_target = (struct globus_i_xio_target_s *)
                    globus_malloc(sizeof(struct globus_i_xio_target_s));
    if(l_target == NULL)
    {
        return GlobusXIOErrorMemoryAlloc("globus_xio_target_init");
    }

    res = globus_i_xio_target_init(
              l_target,
              target_attr,
              contact_string,
              stack);
    if(res != GLOBUS_SUCCESS)
    {
        globus_free(l_target);
        return res;
    }

    return GLOBUS_SUCCESS;
}

    GlobusXIOStackGetDrivers(l_stack, driver_list);
    stack_size = globus_list_size(driver_list);
    if(stack_size == 0)
    {
        res = GlobusXIOErrorInvalidStack("globus_xio_target_init");
        goto err;
    }

    target_stack = (struct globus_i_xio_target_stack_s *)
                        globus_malloc(
                            sizeof(struct globus_i_xio_target_stack_s) *
                                stack_size);
    if(target_stack == NULL)
    {
        res = GlobusXIOErrorMemoryAlloc("globus_xio_target_init");
        goto err;
    }

    ndx = 0;
    for(list = driver_list;
    !globus_list_empty(list);
        list = globus_list_rest(list))
    {
        target_stack[ndx].driver = globus_list_first(list);

        /* pull driver specific info out of target attr */
        driver_attr = globus_l_xio_attr_find_driver(
                        l_target_attr,
                        target_stack[ndx].driver);

        GlobusXIODriverTargetInit(
            res,
            target_stack[ndx].driver,
            &target_stack[ndx].target,
            driver_attr,
            contact_string);
        if(res != GLOBUS_SUCCESS)
        {
            globus_result_t                             res2;

            /* loop back through and destroy all inited targets */
            for(ctr = 0; ctr < ndx; ctr++)
            {
                /* ignore the result, but it must be passed */
                GlobusXIODriverTargetDestroy(
                    res2,
                    target_stack[ndx].driver,
                    target_stack[ndx].target);
            }
            goto err;
        }

        ndx++;
    }
    /* hell has broken loose if these are not equal */
    globus_assert(ndx == stack_size);

    l_target->target_stack = target_stack;
    l_target->stack_size = stack_size;
    *target = l_target;

