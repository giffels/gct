/**
 *  This program tests the globus_ftp_control_data library
 */
#include <globus_ftp_control.h>
#include <globus_common.h>
#include <string.h>
#include "test_common.h"

#define MAX_PLEVEL                              10
#define TEST_ITEREATION                         4
#define WRITE_CHUNK_COUNT                       32

static globus_bool_t                            g_send_eof = GLOBUS_TRUE;

typedef void (*set_handle_mode_cb_t)(
    globus_ftp_control_handle_t *               handle,
    int                                         plevel);

typedef struct data_test_info_s
{
    char                                        fname[512];
    FILE *                                      fin;
    FILE *                                      fout;
    ftp_test_monitor_t *                        monitor;
    int                                         bb_len;
} data_test_info_t;

static int                                      g_test_count = 0;
static char *                                   g_test_file = GLOBUS_NULL;
static char *                                   g_tmp_file = "/tmp/globus_ftp_control_data_test_tmp_file.tmp";

globus_result_t
cache_test(
    set_handle_mode_cb_t                       mode_cb,
    int                                        plevel);

globus_result_t
reuse_handles_test(
    set_handle_mode_cb_t                       mode_cb,
    int                                        plevel);

void
test_result(
    globus_result_t                             result,
    char *                                      msg,
    int                                         line_num);

globus_result_t
transfer_test(
    set_handle_mode_cb_t                       mode_cb,
    int                                        plevel);

globus_result_t
big_buffer_test(
    set_handle_mode_cb_t                       mode_cb,
    int                                        plevel);

void
failure_end(
    char *                                      msg);

void
connect_read_callback(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    unsigned int                                stripe_ndx,
    globus_bool_t                               resuse,
    globus_object_t *                           error);

void
connect_write_zero_eof_callback(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    unsigned int                                stripe_ndx,
    globus_bool_t                               resuse,
    globus_object_t *                           error);

void
connect_write_callback(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    unsigned int                                stripe_ndx,
    globus_bool_t                               resuse,
    globus_object_t *                           error);

void
data_read_callback(
    void *                                      callback_arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *                           error,
    globus_byte_t *                             buffer,
    globus_size_t                               length,
    globus_off_t                                offset,
    globus_bool_t                               eof);

void
data_read_big_buffer_callback(
    void *                                      callback_arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *                           error,
    globus_byte_t *                             buffer,
    globus_size_t                               length_read,
    globus_off_t                                offset,
    globus_bool_t                               eof);

void
data_write_callback(
    void *                                      callback_arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *                           error,
    globus_byte_t *                             buffer,
    globus_size_t                               length,
    globus_off_t                                offset,
    globus_bool_t                               eof);

void
connect_write_big_buffer_callback(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    unsigned int                                stripe_ndx,
    globus_bool_t                               resuse,
    globus_object_t *                           error);

void
connect_read_big_buffer_callback(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    unsigned int                                stripe_ndx,
    globus_bool_t                               resuse,
    globus_object_t *                           error);

void 
binary_eb_mode(
    globus_ftp_control_handle_t *              handle,
    int                                        plevel);

void 
binary_stream_mode(
    globus_ftp_control_handle_t *              handle,
    int                                        plevel);

void
force_close_cb( 
    void *                                     user_arg,
    globus_ftp_control_handle_t *              handle,
    globus_object_t *                          error)
{
    ftp_test_monitor_t *                       monitor;

    monitor = (ftp_test_monitor_t *) user_arg;

    globus_mutex_lock(&monitor->mutex);
    {
        monitor->done = GLOBUS_TRUE;
        globus_cond_signal(&monitor->cond);
    }
    globus_mutex_unlock(&monitor->mutex);
}

int 
main(
    int                                         argc,
    char *                                      argv[])
{ 
    globus_result_t                             res;
    int                                         ctr;
    int						rc;
    int                                         plevel;

    g_test_file = argv[0];
    for(ctr = 0; ctr < argc; ctr++)
    {
        if(strcmp(argv[ctr], "-verbose") == 0)
        {
            if(ctr + 1 >= argc)
            {
                verbose_print_level = 1;
            }
            else
            {
                verbose_print_level = atoi(argv[ctr+1]);
                ctr++;
            }
        }
        else if(strcmp(argv[ctr], "--file") == 0 && ctr + 1 <= argc)
        {
            g_test_file = argv[ctr + 1];
            ctr++;
        }
        else if(strcmp(argv[ctr], "--tmp_file") == 0 && ctr + 1 <= argc)
        {
            g_tmp_file = argv[ctr + 1];
            ctr++;
        }
    }

    /*
     *  activate
     */
    rc = globus_module_activate(GLOBUS_FTP_CONTROL_MODULE);
    if(rc) res = globus_error_put(GLOBUS_ERROR_NO_INFO);
    else   res = GLOBUS_SUCCESS;

    test_result(res, "globus_module_activate failed", __LINE__);

    g_test_count++;
    verbose_printf(1, "------------------------------------\n");
    verbose_printf(1, "running transfer test in stream mode\n");
    transfer_test(binary_stream_mode, 1);
    verbose_printf(1, "transfer test in stream mode passed\n");
    verbose_printf(1, "------------------------------------\n");

    g_test_count++;
    verbose_printf(1, "------------------------------------\n");
    verbose_printf(1, "running reuse handles test in stream mode.\n");
    reuse_handles_test(binary_stream_mode, 1);
    verbose_printf(1, "reuse handles test in stream mode passed.\n");
    verbose_printf(1, "------------------------------------\n");

    g_test_count++;
    verbose_printf(1, "------------------------------------\n");
    verbose_printf(1, "running big buffer test in stream mode\n");
    big_buffer_test(binary_stream_mode, 1);
    verbose_printf(1, "big buffer in stream mode passed\n");
    verbose_printf(1, "------------------------------------\n");

    g_test_count++;
    verbose_printf(1, "------------------------------------\n");
    verbose_printf(1, "running transfer test in eb mode\n");
    for(plevel = 1; plevel <= MAX_PLEVEL; plevel++)
    {
        verbose_printf(2, "parallel level %d\n", plevel);
        transfer_test(binary_eb_mode, plevel);
    }
    verbose_printf(1, "transfer test in eb mode passed\n");
    verbose_printf(1, "------------------------------------\n");

    g_test_count++;
    verbose_printf(1, "------------------------------------\n");
    verbose_printf(1, "running reuse handles test in eb mode.\n");
    for(plevel = 1; plevel <= MAX_PLEVEL; plevel++)
    {
        verbose_printf(2, "parallel level %d\n", plevel);
        reuse_handles_test(binary_eb_mode, plevel);
    }
    verbose_printf(1, "reuse handles test in eb mode passed.\n");
    verbose_printf(1, "------------------------------------\n");

    g_test_count++;
    g_send_eof = GLOBUS_FALSE;
    verbose_printf(1, "------------------------------------\n");
    verbose_printf(1, "send eof running transfer test in eb mode\n");
    for(plevel = 1; plevel <= MAX_PLEVEL; plevel++)
    {
        verbose_printf(2, "parallel level %d\n", plevel);
        transfer_test(binary_eb_mode, plevel);
    }
    g_send_eof = GLOBUS_TRUE;
    verbose_printf(1, "send eof transfer test in eb mode passed\n");
    verbose_printf(1, "------------------------------------\n");

    g_test_count++;
    g_send_eof = GLOBUS_FALSE;
    verbose_printf(1, "------------------------------------\n");
    verbose_printf(1, "send eof running reuse handle test in eb mode\n");
    for(plevel = 1; plevel <= MAX_PLEVEL; plevel++)
    {
        verbose_printf(2, "parallel level %d\n", plevel);
        reuse_handles_test(binary_eb_mode, plevel);
    }
    g_send_eof = GLOBUS_TRUE;
    verbose_printf(1, "send eof reuse handle test in eb mode passed\n");
    verbose_printf(1, "------------------------------------\n");

    g_test_count++;
    verbose_printf(1, "------------------------------------\n");
    verbose_printf(1, "running big buffer test in eb mode\n");
    for(plevel = 1; plevel <= MAX_PLEVEL; plevel++)
    {
        verbose_printf(2, "parallel level %d\n", plevel);
        big_buffer_test(binary_eb_mode, plevel);  
    }
    verbose_printf(1, "big buffer in eb mode passed\n");
    verbose_printf(1, "------------------------------------\n");

    g_test_count++;
    verbose_printf(1, "------------------------------------\n");
    verbose_printf(1, "running cache test in eb mode\n");
    for(plevel = 1; plevel <= MAX_PLEVEL; plevel++)
    {
        verbose_printf(2, "parallel level %d\n", plevel);
        cache_test(binary_eb_mode, plevel);
    }
    verbose_printf(1, "cache in eb mode passed\n");
    verbose_printf(1, "------------------------------------\n");

    g_test_count++;
    g_send_eof = GLOBUS_FALSE;
    verbose_printf(1, "--------------------------------------\n");
    verbose_printf(1, "send_eof running cache test in eb mode\n");
    for(plevel = 1; plevel <= MAX_PLEVEL; plevel++)
    {
        verbose_printf(2, "parallel level %d\n", plevel);
        cache_test(binary_eb_mode, plevel);
    }
    g_send_eof = GLOBUS_TRUE;
    verbose_printf(1, "send_eof cache in eb mode passed\n");
    verbose_printf(1, "-------------------------------------\n");

    rc = globus_module_deactivate(GLOBUS_FTP_CONTROL_MODULE);
    if(rc) res = globus_error_put(GLOBUS_ERROR_NO_INFO);
    else   res = GLOBUS_SUCCESS;
    test_result(res, "deactivate", __LINE__);

    verbose_printf(1, "%d tests passed.\n", g_test_count);
    printf("Success.\n");
    return 0;
}

void
data_send_eof_cb(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    globus_object_t *                           error)
{
    ftp_test_monitor_t *                        done_monitor;

    done_monitor = (ftp_test_monitor_t *)callback_arg;

    globus_mutex_lock(&done_monitor->mutex);
    {
        done_monitor->done = GLOBUS_TRUE;
        globus_cond_signal(&done_monitor->cond);
    }
    globus_mutex_unlock(&done_monitor->mutex);
}


globus_result_t
test_send_eof(
    globus_ftp_control_handle_t *              port_handle)
{
    ftp_test_monitor_t                         done_monitor;
    globus_result_t                            res;
    int                                        x = 0;

    ftp_test_monitor_init(&done_monitor);

    res = globus_ftp_control_data_send_eof(
              port_handle,
              &x,
              1,
              GLOBUS_TRUE,
              data_send_eof_cb,
              (void *)&done_monitor);

    if(res != GLOBUS_SUCCESS)
    { 
        return res;
    }
    globus_mutex_lock(&done_monitor.mutex);
    {
        while(!done_monitor.done)
        { 
            globus_cond_wait(&done_monitor.cond, &done_monitor.mutex);
        }
    }
    globus_mutex_unlock(&done_monitor.mutex);

    return GLOBUS_SUCCESS;
}

/*
 *  test several read using the big buffer test
 */
globus_result_t
big_buffer_test(
    set_handle_mode_cb_t                       mode_cb,
    int                                        plevel)
{
    int                                        ctr;
    globus_result_t                            res;
    globus_ftp_control_host_port_t             host_port;
    globus_ftp_control_handle_t                port_handle;
    globus_ftp_control_handle_t                pasv_handle;
    ftp_test_monitor_t                         done_monitor;
    data_test_info_t *                         test_info;

    ftp_test_monitor_init(&done_monitor);
    done_monitor.result = GLOBUS_SUCCESS;

    test_info = (data_test_info_t *)
        globus_malloc(sizeof(data_test_info_t));
    test_info->monitor = &done_monitor;
    strcpy(test_info->fname, g_tmp_file);

    res = globus_i_ftp_control_data_cc_init(&pasv_handle);
    test_result(res, "pasv handle init", __LINE__);
    res = globus_i_ftp_control_data_cc_init(&port_handle);
    test_result(res, "port handle init", __LINE__);

    for(ctr = 0; ctr < TEST_ITEREATION; ctr++)
    {
        done_monitor.done = GLOBUS_FALSE;
        done_monitor.count = 0;

        memset(&host_port, '\0', sizeof(host_port));

        globus_ftp_control_host_port_init(&host_port, "localhost", 0);
        res = globus_ftp_control_local_pasv(&pasv_handle, &host_port);
        test_result(res, "local pasv", __LINE__);
        res = globus_ftp_control_local_port(&port_handle, &host_port);
        test_result(res, "local port", __LINE__);

        mode_cb(&pasv_handle, plevel);
        mode_cb(&port_handle, plevel);

        /*
         *  calling connect read/write() will get the ball rolling
         */
        res = globus_ftp_control_data_connect_read(
                  &pasv_handle,
                  connect_read_big_buffer_callback,
                  (void *)test_info);
        test_result(res, "connect_read", __LINE__);
        res = globus_ftp_control_data_connect_write(
                  &port_handle,
                  connect_write_big_buffer_callback,
                  (void *)test_info);
        test_result(res, "connect_write", __LINE__);

        verbose_printf(3, "waiting for transfer.\n");
        globus_mutex_lock(&done_monitor.mutex);
        {
            while(done_monitor.count < 2 && 
                  !done_monitor.done)
            {
                globus_cond_wait(&done_monitor.cond, &done_monitor.mutex);
            }
        }
        globus_mutex_unlock(&done_monitor.mutex);
    }

    done_monitor.done = GLOBUS_FALSE;
    res = globus_ftp_control_data_force_close(
              &pasv_handle,
              force_close_cb,
              (void *)&done_monitor);
    if(res == GLOBUS_SUCCESS)
    {
        globus_mutex_lock(&done_monitor.mutex);
        {
            while(!done_monitor.done)
            {
                globus_cond_wait(&done_monitor.cond, &done_monitor.mutex);
            }
        }
        globus_mutex_unlock(&done_monitor.mutex);
    }
    res = globus_i_ftp_control_data_cc_destroy(&pasv_handle);
    test_result(res, "destroy", __LINE__);

    done_monitor.done = GLOBUS_FALSE;
    res = globus_ftp_control_data_force_close(
              &port_handle,
              force_close_cb,
              (void *)&done_monitor);
    if(res == GLOBUS_SUCCESS)
    {
        globus_mutex_lock(&done_monitor.mutex);
        {
            while(!done_monitor.done)
            {
                globus_cond_wait(&done_monitor.cond, &done_monitor.mutex);
            }
        }
        globus_mutex_unlock(&done_monitor.mutex);
    }
    res = globus_i_ftp_control_data_cc_destroy(&port_handle);
    test_result(res, "destroy", __LINE__);

    globus_free(test_info);

    return GLOBUS_SUCCESS;
}

globus_result_t
reuse_handles_test(
    set_handle_mode_cb_t                       mode_cb,
    int                                        plevel)
{
    int ctr;
    globus_result_t                            res;
    globus_ftp_control_host_port_t             host_port;
    globus_ftp_control_handle_t                port_handle;
    globus_ftp_control_handle_t                pasv_handle;
    ftp_test_monitor_t                         done_monitor;
    data_test_info_t *                         test_info;
    int                                        port_connections;
    int                                        pasv_connections;
    globus_ftp_control_data_connect_callback_t connect_cb;

    ftp_test_monitor_init(&done_monitor);
    done_monitor.result = GLOBUS_SUCCESS;

    test_info = (data_test_info_t *)
        globus_malloc(sizeof(data_test_info_t));
    test_info->monitor = &done_monitor;
    strcpy(test_info->fname, g_tmp_file);

    res = globus_i_ftp_control_data_cc_init(&pasv_handle);
    test_result(res, "pasv handle init", __LINE__);
    res = globus_i_ftp_control_data_cc_init(&port_handle);
    test_result(res, "port handle init", __LINE__);

    if(!g_send_eof)
    {
        res = globus_ftp_control_local_send_eof(
                  &port_handle,
                  GLOBUS_FALSE);
        test_result(res, "local_send_eof()", __LINE__);
    }

    connect_cb = connect_write_callback;
    for(ctr = 0; ctr < TEST_ITEREATION * 2; ctr++)
    {
        done_monitor.done = GLOBUS_FALSE;
        done_monitor.count = 0;

        memset(&host_port, '\0', sizeof(host_port));
        globus_ftp_control_host_port_init(&host_port, "localhost", 0);
        res = globus_ftp_control_local_pasv(&pasv_handle, &host_port);
        test_result(res, "local pasv", __LINE__);
        res = globus_ftp_control_local_port(&port_handle, &host_port);
        test_result(res, "local port", __LINE__);

        mode_cb(&pasv_handle, plevel);
        mode_cb(&port_handle, plevel);

        /*
         *  calling connect read/write() will get the ball rolling
         */
        res = globus_ftp_control_data_connect_read(
                  &pasv_handle,
                  connect_read_callback,
                  (void *)test_info);
        test_result(res, "connect_read", __LINE__);
        res = globus_ftp_control_data_connect_write(
                  &port_handle,
                  connect_cb,
                  (void *)test_info);
        test_result(res, "connect_write", __LINE__);

        verbose_printf(3, "waiting for transfer.\n");
        res = globus_ftp_control_data_get_total_data_channels(
                    &pasv_handle,
                    &pasv_connections,
                    0);
        res = globus_ftp_control_data_get_total_data_channels(
                    &port_handle,
                    &port_connections,
                    0);

        verbose_printf(3, 
           "pasv_connection count = %d, port connection count = %d\n",
            pasv_connections, port_connections);
        globus_mutex_lock(&done_monitor.mutex);
        {
            while(done_monitor.count < 2 && 
                  !done_monitor.done)
            {
                globus_cond_wait(&done_monitor.cond, &done_monitor.mutex);
            }
        }
        globus_mutex_unlock(&done_monitor.mutex);

        if(ctr == TEST_ITEREATION)
        {
            verbose_printf(2,
                "starting zero eof callback\n");
            connect_cb = connect_write_zero_eof_callback;
        }
    }

    done_monitor.done = GLOBUS_FALSE;
    res = globus_ftp_control_data_force_close(
              &pasv_handle,
              force_close_cb,
              (void *)&done_monitor);
    if(res == GLOBUS_SUCCESS)
    {
        globus_mutex_lock(&done_monitor.mutex);
        {
            while(!done_monitor.done)
            {
                globus_cond_wait(&done_monitor.cond, &done_monitor.mutex);
            }
        }
        globus_mutex_unlock(&done_monitor.mutex);
    }

    globus_free(test_info);
    res = globus_i_ftp_control_data_cc_destroy(&pasv_handle);
    test_result(res, "destroy handle", __LINE__);

    done_monitor.done = GLOBUS_FALSE;
    res = globus_ftp_control_data_force_close(
              &port_handle,
              force_close_cb,
              (void *)&done_monitor);
    if(res == GLOBUS_SUCCESS)
    {
        globus_mutex_lock(&done_monitor.mutex);
        {
            while(!done_monitor.done)
            {
                globus_cond_wait(&done_monitor.cond, &done_monitor.mutex);
            }
        }
        globus_mutex_unlock(&done_monitor.mutex);
    }

    res = globus_i_ftp_control_data_cc_destroy(&port_handle);
    test_result(res, "destroy", __LINE__);

    return GLOBUS_SUCCESS;
}

void 
binary_eb_mode(
    globus_ftp_control_handle_t *               handle,
    int                                         plevel)
{
    globus_result_t                             res;
    globus_ftp_control_parallelism_t            parallelism;

    parallelism.mode = GLOBUS_FTP_CONTROL_PARALLELISM_FIXED;
    parallelism.fixed.size = plevel;

    res = globus_ftp_control_local_type(
              handle, 
              GLOBUS_FTP_CONTROL_TYPE_IMAGE, 
              0);
    test_result(res, "local_type", __LINE__);

    res = globus_ftp_control_local_mode(
              handle, 
              GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK);
    test_result(res, "local_mode", __LINE__);

    res = globus_ftp_control_local_parallelism(
              handle, 
              &parallelism);
    test_result(res, "local_mode", __LINE__);
}

void 
binary_stream_mode(
    globus_ftp_control_handle_t *               handle,
    int                                         plevel)
{
    globus_result_t                             res;

    res = globus_ftp_control_local_type(
              handle, 
              GLOBUS_FTP_CONTROL_TYPE_IMAGE, 
              0);
    test_result(res, "local_type", __LINE__);

    res = globus_ftp_control_local_mode(
              handle, 
              GLOBUS_FTP_CONTROL_MODE_STREAM);
    test_result(res, "local_mode", __LINE__);
}

globus_result_t
cache_test(
    set_handle_mode_cb_t                       mode_cb,
    int                                        plevel)
{
    int ctr;
    globus_result_t                            res;
    globus_ftp_control_host_port_t             host_port;
    globus_ftp_control_handle_t                port_handle;
    globus_ftp_control_handle_t                pasv_handle;
    ftp_test_monitor_t                         done_monitor;
    data_test_info_t *                         test_info;

    ftp_test_monitor_init(&done_monitor);
    done_monitor.result = GLOBUS_SUCCESS;

    test_info = (data_test_info_t *)
        globus_malloc(sizeof(data_test_info_t));
    test_info->monitor = &done_monitor;
    strcpy(test_info->fname, g_tmp_file);

    res = globus_i_ftp_control_data_cc_init(&pasv_handle);
    test_result(res, "pasv handle init", __LINE__);
    res = globus_i_ftp_control_data_cc_init(&port_handle);
    test_result(res, "port handle init", __LINE__);

    host_port.port = 0;
    globus_ftp_control_host_port_init(&host_port, "localhost", 0);
    res = globus_ftp_control_local_pasv(&pasv_handle, &host_port);
    test_result(res, "local pasv", __LINE__);
    res = globus_ftp_control_local_port(&port_handle, &host_port);
    test_result(res, "local port", __LINE__);

    mode_cb(&pasv_handle, plevel);
    mode_cb(&port_handle, plevel);

    if(!g_send_eof)
    {
        res = globus_ftp_control_local_send_eof(
                  &port_handle,
                  GLOBUS_FALSE);
        test_result(res, "local_send_eof()", __LINE__);
    }

    for(ctr = 0; ctr < TEST_ITEREATION; ctr++)
    {
        done_monitor.done = GLOBUS_FALSE;
        done_monitor.count = 0;

        /*
         *  calling connect read/write() will get the ball rolling
         */
        res = globus_ftp_control_data_connect_read(
                  &pasv_handle,
                  connect_read_callback,
                  (void *)test_info);
        test_result(res, "connect_read", __LINE__);
        res = globus_ftp_control_data_connect_write(
                  &port_handle,
                  connect_write_callback,
                  (void *)test_info);
        test_result(res, "connect_write", __LINE__);

        verbose_printf(3, "waiting for transfer.\n");
        globus_mutex_lock(&done_monitor.mutex);
        {
            while(done_monitor.count < 2 && 
                  !done_monitor.done)
            {
                globus_cond_wait(&done_monitor.cond, &done_monitor.mutex);
            }
        }
        globus_mutex_unlock(&done_monitor.mutex);
    }

    done_monitor.done = GLOBUS_FALSE;
    res = globus_ftp_control_data_force_close(
              &pasv_handle,
              force_close_cb,
              (void *)&done_monitor);
    if(res == GLOBUS_SUCCESS)
    {
        globus_mutex_lock(&done_monitor.mutex);
        {
            while(!done_monitor.done)
            {
                globus_cond_wait(&done_monitor.cond, &done_monitor.mutex);
            }
        }
        globus_mutex_unlock(&done_monitor.mutex);
    }

    globus_free(test_info);
    res = globus_i_ftp_control_data_cc_destroy(&pasv_handle);
    test_result(res, "destroy handle", __LINE__);

    done_monitor.done = GLOBUS_FALSE;
    res = globus_ftp_control_data_force_close(
              &port_handle,
              force_close_cb,
              (void *)&done_monitor);
    if(res == GLOBUS_SUCCESS)
    {
        globus_mutex_lock(&done_monitor.mutex);
        {
            while(!done_monitor.done)
            {
                globus_cond_wait(&done_monitor.cond, &done_monitor.mutex);
            }
        }
        globus_mutex_unlock(&done_monitor.mutex);
    }

    res = globus_i_ftp_control_data_cc_destroy(&port_handle);
    test_result(res, "destroy", __LINE__);

    return GLOBUS_SUCCESS;
}
/*
 *  transfer test
 *  -------------
 *  This test local creates several pasv and port handles and connects them.
 *  It then sends data to all of them and compares the data on both ends
 *  if the trasfers.  In order to ease the testing of some of the clean up
 *  this test does leak some memory.
 * 
 *  It tests the control library data code for:
 *  1) read and write functionality for data integrity
 *  2) functionality when multiple handles are simaltaneously being used.
 *  3) reads of sizes greater than and smaller than the size of the extended
 *     block.
 *  4) clean up.  At the end of half the transfers close() and destroy() 
 *     are called.  The other half leave the clean up up to deactivate.
 */
globus_result_t
transfer_test(
    set_handle_mode_cb_t                    mode_cb,
    int                                     plevel)
{
    int                                     ctr;
    globus_result_t                         res;
    globus_ftp_control_host_port_t          host_port;
    globus_ftp_control_handle_t *           port_handle;
    globus_ftp_control_handle_t *           pasv_handle;
    ftp_test_monitor_t                      done_monitor;
    data_test_info_t *                      test_info;
    data_test_info_t                        test_info_array[TEST_ITEREATION];
    globus_ftp_control_handle_t             port_handle_array[TEST_ITEREATION];
    globus_ftp_control_handle_t             pasv_handle_array[TEST_ITEREATION];

    ftp_test_monitor_init(&done_monitor);

    done_monitor.result = GLOBUS_SUCCESS;
    for(ctr = 0; ctr < TEST_ITEREATION; ctr++)
    {
        /*
         *  initialize test info structure
         */
        test_info = &test_info_array[ctr];
        test_info->monitor = &done_monitor;
        strcpy(test_info->fname, g_tmp_file);
        strcat(test_info->fname, ".   ");
        sprintf(&test_info->fname[strlen(test_info->fname) - 3],
                "%d", ctr);
        /*
         *  these will be freed in the final callback
         */
        port_handle = &port_handle_array[ctr];
        pasv_handle = &pasv_handle_array[ctr];

        res = globus_i_ftp_control_data_cc_init(pasv_handle);
        test_result(res, "pasv handle init", __LINE__);
     
        res = globus_i_ftp_control_data_cc_init(port_handle);
        test_result(res, "port handle init", __LINE__);

        /*
         * local_port/pasv()
         */
        globus_ftp_control_host_port_init(&host_port, "localhost", 0);
        res = globus_ftp_control_local_pasv(pasv_handle, &host_port);
        test_result(res, "local pasv", __LINE__);
        res = globus_ftp_control_local_port(port_handle, &host_port);
        test_result(res, "local port", __LINE__);

        mode_cb(pasv_handle, plevel);
        mode_cb(port_handle, plevel);
 
        if(!g_send_eof)
        {
            res = globus_ftp_control_local_send_eof(
                      port_handle,
                      GLOBUS_FALSE);
            test_result(res, "local_send_eof()", __LINE__);
        }
        /*
         *  calling connect read/write() will get the ball rolling
         */
        res = globus_ftp_control_data_connect_read(
                  pasv_handle,
                  connect_read_callback,
                  (void *)test_info);
        test_result(res, "connect_read", __LINE__);
        res = globus_ftp_control_data_connect_write(
                  port_handle,
                  connect_write_callback,
                  (void *)test_info);
        test_result(res, "connect_write", __LINE__);
    }

    /*
     *  wait for end
     */
    verbose_printf(3, "waiting for end\n");
    globus_mutex_lock(&done_monitor.mutex);
    {
        while(done_monitor.count < (TEST_ITEREATION*2) && !done_monitor.done)
        {
            globus_cond_wait(&done_monitor.cond, &done_monitor.mutex);
        }
    }
    globus_mutex_unlock(&done_monitor.mutex);

    /*
     *  clean up
     */
    for(ctr = 0; ctr < TEST_ITEREATION; ctr++)
    {
        done_monitor.done = GLOBUS_FALSE;
        res = globus_ftp_control_data_force_close(
                  &pasv_handle_array[ctr],
                  force_close_cb,
                  (void *)&done_monitor);
        if(res == GLOBUS_SUCCESS)
        {
            globus_mutex_lock(&done_monitor.mutex);
            { 
                while(!done_monitor.done)
                {
                    globus_cond_wait(&done_monitor.cond, &done_monitor.mutex);
                }
            }
            globus_mutex_unlock(&done_monitor.mutex);
        }
        res = globus_i_ftp_control_data_cc_destroy(&pasv_handle_array[ctr]);
        test_result(res, "destroy", __LINE__);

        done_monitor.done = GLOBUS_FALSE;
        res = globus_ftp_control_data_force_close(
                  &port_handle_array[ctr],
                  force_close_cb,
                  (void *)&done_monitor);
        if(res == GLOBUS_SUCCESS)
        {
            globus_mutex_lock(&done_monitor.mutex);
            {
                while(!done_monitor.done)
                {
                    globus_cond_wait(&done_monitor.cond, &done_monitor.mutex);
                }
            }
            globus_mutex_unlock(&done_monitor.mutex);
        }
        res = globus_i_ftp_control_data_cc_destroy(&port_handle_array[ctr]);
        test_result(res, "destroy", __LINE__);
    }
    verbose_printf(3, "ending\n");

    return GLOBUS_SUCCESS;
}

/*
 *  want to test smaller than the write block, and bigger than the write
 *  block
 */
void
connect_read_big_buffer_callback(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    unsigned int                                stripe_ndx,
    globus_bool_t                               resuse,
    globus_object_t *                           error)
{
    data_test_info_t *                         test_info;
    struct stat                                stat_info;
    int                                        file_size;
    globus_byte_t *                            buf;
    globus_result_t                            res;

    test_info = (data_test_info_t *)callback_arg;
   
    verbose_printf(3, 
        "connect_read_big_buffer_callback() : start\n"); 
    if(error != GLOBUS_NULL)
    {
        test_result(globus_error_put(error), 
                    "connect_read_big_buffer_callback error", __LINE__);
    }

    globus_mutex_lock(&test_info->monitor->mutex);
    {
        char                             sys_cmd[1024];

        sprintf(sys_cmd, "cp %s %s", g_test_file, test_info->fname);
        system(sys_cmd);

        test_info->fout = fopen(test_info->fname, "r+");

        if(test_info->fout == GLOBUS_NULL)
        {
            failure_end("fopen failed\n");
        }
        if(stat(g_test_file, &stat_info) < 0)
        {
            failure_end("stat failed\n");
        }
        file_size = stat_info.st_size;
        test_info->bb_len = file_size;

        buf = (globus_byte_t *)malloc(file_size);
        res = globus_ftp_control_data_read_all(
                  handle,
                  buf,
                  file_size,
                  data_read_big_buffer_callback,
                  (void *)test_info);
        test_result(res, "data_read_all", __LINE__);
    }
    globus_mutex_unlock(&test_info->monitor->mutex);
}

/*
 *  want to test smaller than the write block, and bigger than the write
 *  block
 */
void
connect_read_callback(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    unsigned int                                stripe_ndx,
    globus_bool_t                               resuse,
    globus_object_t *                           error)
{
    data_test_info_t *                         test_info;
    struct stat                                stat_info;
    int                                        write_blk_size;
    int                                        blk_size;
    globus_byte_t *                            buf;
    globus_result_t                            res;

    test_info = (data_test_info_t *)callback_arg;
   
    verbose_printf(3, "connect_read_callback() : start\n"); 
    if(error != GLOBUS_NULL)
    {
        test_result(globus_error_put(error), "connect_read_callback error"
              , __LINE__);
    }

    globus_mutex_lock(&test_info->monitor->mutex);
    {
        test_info->fout = fopen(test_info->fname, "w");
        if(test_info->fout == GLOBUS_NULL)
        {
            failure_end("fopen failed\n");
        }
        if(stat(g_test_file, &stat_info) < 0)
        {
            failure_end("stat failed\n");
        }
        write_blk_size = stat_info.st_size / WRITE_CHUNK_COUNT + 1; 
        blk_size = write_blk_size / 2 + 1;

        buf = (globus_byte_t *)malloc(blk_size);
        res = globus_ftp_control_data_read(
                  handle,
                  buf,
                  blk_size,
                  data_read_callback,
                  (void *)test_info);
        test_result(res, "data_read", __LINE__);
    }
    globus_mutex_unlock(&test_info->monitor->mutex);

}

void
data_read_big_buffer_callback(
    void *                                      callback_arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *                           error,
    globus_byte_t *                             buffer,
    globus_size_t                               length_read,
    globus_off_t                                offset_read,
    globus_bool_t                               eof)
{
    data_test_info_t *                          test_info; 

    verbose_printf(3, 
        "data_read_big_buffer_callback():start %d %d %d\n", 
         length_read, offset_read, eof);

    if(error != GLOBUS_NULL)
    {
        test_result(globus_error_put(error), "big_buffer_callback", __LINE__);
    }

    test_info = (data_test_info_t *)callback_arg;

    globus_mutex_lock(&test_info->monitor->mutex);
    {
        if(!eof)
        {
            verbose_printf(3, "intermediate big buffer callback\n");
        }
        else
        {
            char                              sys_cmd[512];

            verbose_printf(3, "eof big buffer callback\n");
            /* write out the entire buffer */
            
            if(fwrite(buffer, 1, test_info->bb_len, test_info->fout) 
                                                     != test_info->bb_len)
            {
                failure_end("fwrite failed\n");
            }
            verbose_printf(3, "closing the out stream\n");
            fflush(test_info->fout);
            fclose(test_info->fout);

            sprintf(sys_cmd, "diff %s %s", test_info->fname, g_test_file);
            if(system(sys_cmd) != 0)
            {
                verbose_printf(1, "files are not the same\n");
                test_info->monitor->done = GLOBUS_TRUE;
                test_info->monitor->result = 
                      globus_error_put(GLOBUS_ERROR_NO_INFO);
                globus_cond_signal(&test_info->monitor->cond);
            }
            else
            {
                verbose_printf(3, "files are the same\n");
                test_info->monitor->count++;
                globus_cond_signal(&test_info->monitor->cond);
            }
            globus_free(buffer);
        }
    }
    globus_mutex_unlock(&test_info->monitor->mutex);
}

void
connect_write_big_buffer_callback(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    unsigned int                                stripe_ndx,
    globus_bool_t                               resuse,
    globus_object_t *                           error)
{
    data_test_info_t *                         test_info;
    struct stat                                stat_info;
    int                                        blk_size;
    int                                        offset = 0;
    int                                        nbyte;
    globus_byte_t *                            buf;
    globus_bool_t                              eof = GLOBUS_FALSE;
    globus_result_t                            res;
    int                                        ctr;

    test_info = (data_test_info_t *)callback_arg;

    verbose_printf(3, "connect_write_callback() : start\n");
    if(error != GLOBUS_NULL)
    {
        verbose_printf(1, "error:%s\n",
            globus_object_printable_to_string(error));
        failure_end("connect_write_callback error\n");
    }

    globus_mutex_lock(&test_info->monitor->mutex);
    {
        test_info->fin = fopen(g_test_file, "r");
        if(test_info->fin == GLOBUS_NULL)
        {
            failure_end("fopen failed\n");
        }
        if(stat(g_test_file, &stat_info) < 0)
        {
            failure_end("stat failed\n");
        }
   
        offset = 0;
        fseek(test_info->fin, offset, SEEK_SET);
        blk_size = stat_info.st_size / WRITE_CHUNK_COUNT + 1; 
        for(ctr = 0; ctr < WRITE_CHUNK_COUNT; ctr++)
        {
            buf = globus_malloc(blk_size);

            /*
             *  read a chunk
             */
            nbyte = fread(buf, 1, blk_size, test_info->fin); 
            if(nbyte < blk_size)
            {
                if(feof(test_info->fin))
                {
                    verbose_printf(3, "registering eof\n");
                    fclose(test_info->fin);
                    eof = GLOBUS_TRUE;
                }
                else
                {
                    failure_end("fread failed\n");
                }
            }
            /*
             *  write a chunk
             */
            verbose_printf(3, "registering a write 0x%x offset=%d length=%d eof=%d\n",
                          buf, offset, nbyte, eof);

            res = globus_ftp_control_data_write(
                      handle,
                      buf,
                      nbyte,
                      offset,
                      eof,
                      data_write_callback, 
                      (void *)test_info);
            test_result(res, "data_write", __LINE__);
            offset += nbyte;
        }
    }
    globus_mutex_unlock(&test_info->monitor->mutex);
}

void 
data_read_callback(
    void *                                      callback_arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *                           error,
    globus_byte_t *                             buffer,
    globus_size_t                               length,
    globus_off_t                                offset,
    globus_bool_t                               eof)
{
    data_test_info_t *                          test_info; 
    globus_result_t                             res;
    int                                         blk_size;
    globus_byte_t *                             buf;

    verbose_printf(3, "data_read_callback() : start\n"); 
    if(error != GLOBUS_NULL)
    {
        failure_end("data_read_callback error\n");
    }
    test_info = (data_test_info_t *)callback_arg;
    globus_mutex_lock(&test_info->monitor->mutex);
    {
        verbose_printf(3, "seeking to %d\n", offset);
        if(fseek(test_info->fout, offset, SEEK_SET) != 0)
        {
            verbose_printf(1, "errno %d %d %d\n", errno, EBADF, EINVAL);
            failure_end("seek failed\n");
        }
        if(fwrite(buffer, 1, length, test_info->fout) != length)
        {
            failure_end("fwrite failed\n");
        }

        if(eof)
        {
            char                              sys_cmd[512];

            verbose_printf(3, "closing the out stream\n");
            fclose(test_info->fout);

            sprintf(sys_cmd, "diff %s %s", test_info->fname, g_test_file);
            if(system(sys_cmd) != 0)
            {
                verbose_printf(1, "files are not the same\n");
                test_info->monitor->done = GLOBUS_TRUE;
                test_info->monitor->result = 
                      globus_error_put(GLOBUS_ERROR_NO_INFO);
                globus_cond_signal(&test_info->monitor->cond);
            }
            else
            {
                verbose_printf(3, "files are the same\n");
                test_info->monitor->count++;
                globus_cond_signal(&test_info->monitor->cond);
            }
        }
        else
        {
            /* add some stuff to the length */
            blk_size = length + (rand() % length);
            buf = globus_malloc(blk_size);
            res = globus_ftp_control_data_read(
                      handle,
                      buf,
                      blk_size,
                      data_read_callback,
                      (void *)test_info);
            test_result(res, "data_read", __LINE__);
        }
    }
    globus_mutex_unlock(&test_info->monitor->mutex);

    if(!eof && length > 0)
    {
        globus_free(buffer);
    }
}

void
connect_write_callback(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    unsigned int                                stripe_ndx,
    globus_bool_t                               resuse,
    globus_object_t *                           error)
{
    data_test_info_t *                         test_info;
    struct stat                                stat_info;
    int                                        blk_size;
    int                                        offset = 0;
    int                                        nbyte;
    globus_byte_t *                            buf;
    globus_bool_t                              eof = GLOBUS_FALSE;
    globus_result_t                            res;
    int                                        ctr;

    test_info = (data_test_info_t *)callback_arg;

    verbose_printf(3, "connect_write_callback() : start\n");
    if(error != GLOBUS_NULL)
    {
        verbose_printf(1, "error:%s\n",
            globus_object_printable_to_string(error));
        failure_end("connect_write_callback error\n");
    }

    globus_mutex_lock(&test_info->monitor->mutex);
    {
        test_info->fin = fopen(g_test_file, "r");
        if(test_info->fin == GLOBUS_NULL)
        {
            failure_end("fopen failed\n");
        }
        if(stat(g_test_file, &stat_info) < 0)
        {
            failure_end("stat failed\n");
        }
       
        blk_size = stat_info.st_size / WRITE_CHUNK_COUNT + 1; 
        for(ctr = 0; ctr < WRITE_CHUNK_COUNT; ctr++)
        {
            buf = globus_malloc(blk_size);

            /*
             *  read a chunk
             */
            nbyte = fread(buf, 1, blk_size, test_info->fin); 
            if(nbyte < blk_size)
            {
                if(feof(test_info->fin))
                {
                    verbose_printf(3, "registering eof\n");
                    fclose(test_info->fin);
                    eof = GLOBUS_TRUE;
                }
                else
                {
                    failure_end("fread failed\n");
                }
            }
            /*
             *  write a chunk
             */
            verbose_printf(3, 
                "registering a write 0x%x offset=%d length=%d eof=%d\n",
                          buf, offset, nbyte, eof);
            res = globus_ftp_control_data_write(
                      handle,
                      buf,
                      nbyte,
                      offset,
                      eof,
                      data_write_callback, 
                      (void *)test_info);
            test_result(res, "data_write", __LINE__);
            offset += nbyte;
        }
    }
    globus_mutex_unlock(&test_info->monitor->mutex);
}


void
connect_write_zero_eof_callback(
    void *                                      callback_arg,
    struct globus_ftp_control_handle_s *        handle,
    unsigned int                                stripe_ndx,
    globus_bool_t                               resuse,
    globus_object_t *                           error)
{
    data_test_info_t *                         test_info;
    struct stat                                stat_info;
    int                                        blk_size;
    int                                        offset = 0;
    int                                        nbyte;
    globus_byte_t *                            buf;
    globus_bool_t                              eof = GLOBUS_FALSE;
    globus_bool_t                              done = GLOBUS_FALSE;
    globus_result_t                            res;
    int                                        ctr;

    test_info = (data_test_info_t *)callback_arg;

    verbose_printf(3, "connect_write_callback() : start\n");
    if(error != GLOBUS_NULL)
    {
        verbose_printf(1, "error:%s\n",
            globus_object_printable_to_string(error));
        failure_end("connect_write_callback error\n");
    }

    globus_mutex_lock(&test_info->monitor->mutex);
    {
        test_info->fin = fopen(g_test_file, "r");
        if(test_info->fin == GLOBUS_NULL)
        {
            failure_end("fopen failed\n");
        }
        if(stat(g_test_file, &stat_info) < 0)
        {
            failure_end("stat failed\n");
        }

        blk_size = stat_info.st_size;
        buf = globus_malloc(blk_size);

        /*
         *  read a chunk
         */
        nbyte = 0;
        while(nbyte < blk_size)
        {
            nbyte += fread(&buf[nbyte], 1, blk_size-nbyte, test_info->fin);
        }

        fclose(test_info->fin);
        /*
         *  write a chunk
         */
        verbose_printf(3, 
            "registering a write 0x%x offset=%d length=%d eof=%d\n",
                          buf, offset, nbyte, eof);
        res = globus_ftp_control_data_write(
                  handle,
                  buf,
                  nbyte,
                  offset,
                  GLOBUS_FALSE,
                  data_write_callback,
                  (void *)test_info);
        test_result(res, "data_write", __LINE__);
        offset += nbyte;
       
        res = globus_ftp_control_data_write(
                  handle,
                  buf,
                  0,
                  offset,
                  GLOBUS_TRUE,
                  data_write_callback,
                  (void *)test_info);
        test_result(res, "data_write", __LINE__);
    }
    globus_mutex_unlock(&test_info->monitor->mutex);
}

void
data_write_callback(
    void *                                      callback_arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *                           error,
    globus_byte_t *                             buffer,
    globus_size_t                               length,
    globus_off_t                                offset,
    globus_bool_t                               eof)
{
    data_test_info_t *                          test_info;
    globus_result_t                             res;

    test_info = (data_test_info_t *)callback_arg;

    verbose_printf(3, "data_write_callback() : start\n");
    if(error != GLOBUS_NULL)
    {
        verbose_printf(1, "error:%s\n",
            globus_object_printable_to_string(error));
        failure_end("data_write_callback error\n");
    }

    if(eof)
    {
        verbose_printf(3, "data_write_callback() : eof has been reached\n");

        if(!g_send_eof)
        {
            res = test_send_eof(handle);
            test_result(res, "send_eof()", __LINE__);
        }

        globus_mutex_lock(&test_info->monitor->mutex);
        {
            test_info->monitor->count++;
            globus_cond_signal(&test_info->monitor->cond);
        }
        globus_mutex_unlock(&test_info->monitor->mutex);
    } 
    if(length > 0 &&!eof)
    {
        globus_free(buffer);
    }
}

void
failure_end(
    char *                                      msg)
{
    verbose_printf(1, "%s\n", msg);
    verbose_printf(1, "test #%d failed\n", g_test_count);
    assert(GLOBUS_FALSE);
}

void
test_result(
    globus_result_t                             res,
    char *                                      msg,
    int                                         line_num)
{
    if(res != GLOBUS_SUCCESS)
    {
        verbose_printf(1, "Line# %d [error]:%s\n", line_num,
            globus_object_printable_to_string(globus_error_get(res)));
        failure_end(msg);
    }
}
