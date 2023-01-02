/*
 * iperf, Copyright (c) 2014-2020, The Regents of the University of
 * California, through Lawrence Berkeley National Laboratory (subject
 * to receipt of any required approvals from the U.S. Dept. of
 * Energy).  All rights reserved.
 *
 * If you have questions about your rights to use or distribute this
 * software, please contact Berkeley Lab's Technology Transfer
 * Department at TTD@lbl.gov.
 *
 * NOTICE.  This software is owned by the U.S. Department of Energy.
 * As such, the U.S. Government has been granted for itself and others
 * acting on its behalf a paid-up, nonexclusive, irrevocable,
 * worldwide license in the Software to reproduce, prepare derivative
 * works, and perform publicly and display publicly.  Beginning five
 * (5) years after the date permission to assert copyright is obtained
 * from the U.S. Department of Energy, and subject to any subsequent
 * five (5) year renewals, the U.S. Government is granted for itself
 * and others acting on its behalf a paid-up, nonexclusive,
 * irrevocable, worldwide license in the Software to reproduce,
 * prepare derivative works, distribute copies to the public, perform
 * publicly and display publicly, and to permit others to do so.
 *
 * This code is distributed under a BSD style license, see the LICENSE
 * file for complete information.
 */
#include <errno.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <arpa/inet.h>

#include "iperf.h"
#include "iperf_api.h"
#include "iperf_util.h"
#include "iperf_locale.h"
#include "iperf_time.h"
#include "net.h"
#include "timer.h"

#if defined(HAVE_TCP_CONGESTION)
#if !defined(TCP_CA_NAME_MAX)
#define TCP_CA_NAME_MAX 16
#endif /* TCP_CA_NAME_MAX */
#endif /* HAVE_TCP_CONGESTION */

int
iperf_create_streams(struct iperf_test *test, int sender)
{
    int i, s;
#if defined(HAVE_TCP_CONGESTION)
    int saved_errno;
#endif /* HAVE_TCP_CONGESTION */
    struct iperf_stream *sp;

    int orig_bind_port = test->bind_port;
    for (i = 0; i < test->num_streams; ++i) {

        test->bind_port = orig_bind_port;
	if (orig_bind_port)
	    test->bind_port += i;

	/* connect socket，即根据tcp和udp 调用不同的api。
	tcp: iperf_tcp_connect()
	udp: iperf_udp_connect()
	*/
	MY_DEBUG("create a new test stream, protocol->connect()....\n");
        if ((s = test->protocol->connect(test)) < 0)
            return -1;

#if defined(HAVE_TCP_CONGESTION)
	if (test->protocol->id == Ptcp) {
	    if (test->congestion) {
		if (setsockopt(s, IPPROTO_TCP, TCP_CONGESTION, test->congestion, strlen(test->congestion)) < 0) {
		    saved_errno = errno;
		    close(s);
		    errno = saved_errno;
		    i_errno = IESETCONGESTION;
		    return -1;
		} 
	    }
	    {
		socklen_t len = TCP_CA_NAME_MAX;
		char ca[TCP_CA_NAME_MAX + 1];
		if (getsockopt(s, IPPROTO_TCP, TCP_CONGESTION, ca, &len) < 0) {
		    saved_errno = errno;
		    close(s);
		    errno = saved_errno;
		    i_errno = IESETCONGESTION;
		    return -1;
		}
		test->congestion_used = strdup(ca);
		if (test->debug) {
		    printf("Congestion algorithm is %s\n", test->congestion_used);
		}
	    }
	}
#endif /* HAVE_TCP_CONGESTION */

	/* 根据client的测试模式，将新建的流添加到相应的set中。
	 * 有write set和read set。
	 */
	if (sender)
	    FD_SET(s, &test->write_set);
	else
	    FD_SET(s, &test->read_set);
	if (s > test->max_fd) test->max_fd = s;

        sp = iperf_new_stream(test, s, sender);
        if (!sp)
            return -1;

        /* Perform the new stream callback */
        if (test->on_new_stream)
            test->on_new_stream(sp);
    }

    return 0;
}

static void
test_timer_proc(TimerClientData client_data, struct iperf_time *nowP)
{
    struct iperf_test *test = client_data.p;

	MY_DEBUG("start\n");

    test->timer = NULL;
    test->done = 1;
}

static void
client_stats_timer_proc(TimerClientData client_data, struct iperf_time *nowP)
{
    struct iperf_test *test = client_data.p;

	MY_DEBUG("start\n");

    if (test->done)
        return;
    if (test->stats_callback)
	test->stats_callback(test);
}

static void
client_reporter_timer_proc(TimerClientData client_data, struct iperf_time *nowP)
{
    struct iperf_test *test = client_data.p;

	MY_DEBUG("start\n");

    if (test->done)
        return;
    if (test->reporter_callback)
	test->reporter_callback(test);
}

static int
create_client_timers(struct iperf_test * test)
{
    struct iperf_time now;
    TimerClientData cd;

    if (iperf_time_now(&now) < 0) {
	i_errno = IEINITTEST;
	return -1;
    }
    cd.p = test;
    test->timer = test->stats_timer = test->reporter_timer = NULL;
    if (test->duration != 0) {
	test->done = 0;
	MY_DEBUG("create client test timers(-t + -O): usecs=%lld\n", ( test->duration + test->omit ) * SEC_TO_US);
        test->timer = tmr_create(&now, test_timer_proc, cd, ( test->duration + test->omit ) * SEC_TO_US, 0);
        if (test->timer == NULL) {
            i_errno = IEINITTEST;
            return -1;
	}
    }

    if (test->stats_interval != 0) {
    	MY_DEBUG("create client stats timers(-i): usecs=%lld\n", (unsigned long long)(test->stats_interval * SEC_TO_US));
        test->stats_timer = tmr_create(&now, client_stats_timer_proc, cd, test->stats_interval * SEC_TO_US, 1);
        if (test->stats_timer == NULL) {
            i_errno = IEINITTEST;
            return -1;
	}
    }

    if (test->reporter_interval != 0) {
    	MY_DEBUG("create client reporter timers(-i): usecs=%lld\n", (unsigned long long)(test->reporter_interval * SEC_TO_US));
        test->reporter_timer = tmr_create(&now, client_reporter_timer_proc, cd, test->reporter_interval * SEC_TO_US, 1);
        if (test->reporter_timer == NULL) {
            i_errno = IEINITTEST;
            return -1;
	}
    }
    return 0;
}

static void
client_omit_timer_proc(TimerClientData client_data, struct iperf_time *nowP)
{
    struct iperf_test *test = client_data.p;

    test->omit_timer = NULL;
    test->omitting = 0;

	MY_DEBUG("start\n");

    iperf_reset_stats(test);
    if (test->verbose && !test->json_output && test->reporter_interval == 0)
        iperf_printf(test, "%s", report_omit_done);

    /* Reset the timers. */
    if (test->stats_timer != NULL)
        tmr_reset(nowP, test->stats_timer);
    if (test->reporter_timer != NULL)
        tmr_reset(nowP, test->reporter_timer);
}

static int
create_client_omit_timer(struct iperf_test * test)
{
    struct iperf_time now;
    TimerClientData cd;

	MY_DEBUG("start\n");

    if (test->omit == 0) {
	test->omit_timer = NULL;
        test->omitting = 0;
    } else {
	if (iperf_time_now(&now) < 0) {
	    i_errno = IEINITTEST;
	    return -1;
	}
	test->omitting = 1;
	cd.p = test;

    	MY_DEBUG("create client omit timers(-O): usecs=%lld\n",  test->omit * SEC_TO_US);
	test->omit_timer = tmr_create(&now, client_omit_timer_proc, cd, test->omit * SEC_TO_US, 0);
	if (test->omit_timer == NULL) {
	    i_errno = IEINITTEST;
	    return -1;
	}
    }
    return 0;
}

int
iperf_handle_message_client(struct iperf_test *test)
{
    int rval;
    int32_t err;

    /*!!! Why is this read() and not Nread()? */
    if ((rval = read(test->ctrl_sck, (char*) &test->state, sizeof(signed char))) <= 0) {
        if (rval == 0) {
            i_errno = IECTRLCLOSE;
            return -1;
        } else {
            i_errno = IERECVMESSAGE;
            return -1;
        }
    }

	MY_DEBUG("client handle server message: test->state:%d\n", test->state);

    switch (test->state) {

        case PARAM_EXCHANGE:
		MY_DEBUG("PARAM_EXCHANGE message\n\n");
            if (iperf_exchange_parameters(test) < 0)
                return -1;
		/* 打印log，实际执行   iperf_on_connect() */
            if (test->on_connect)
                test->on_connect(test);
            break;

        case CREATE_STREAMS:
		MY_DEBUG("CREATE_STREAMS message\n\n");
            if (test->mode == BIDIRECTIONAL)
            {
                if (iperf_create_streams(test, 1) < 0)
                    return -1;
                if (iperf_create_streams(test, 0) < 0)
                    return -1;
            }
            else if (iperf_create_streams(test, test->mode) < 0)
                return -1;
            break;

        case TEST_START:
		MY_DEBUG("TEST_START message\n\n");
            if (iperf_init_test(test) < 0)
                return -1;
            if (create_client_timers(test) < 0)
                return -1;
            if (create_client_omit_timer(test) < 0)
                return -1;
	    if (test->mode)
		if (iperf_create_send_timers(test) < 0)
		    return -1;
            break;

        case TEST_RUNNING:
		MY_DEBUG("TEST_RUNNING message\n\n");
            break;

        case EXCHANGE_RESULTS:
		MY_DEBUG("EXCHANGE_RESULTS message\n\n");
            if (iperf_exchange_results(test) < 0)
                return -1;
            break;

        case DISPLAY_RESULTS:
		MY_DEBUG("DISPLAY_RESULTS message\n\n");
            if (test->on_test_finish)
                test->on_test_finish(test);
            iperf_client_end(test);
            break;

        case IPERF_DONE:
		MY_DEBUG("IPERF_DONE message\n\n");
            break;

        case SERVER_TERMINATE:
		MY_DEBUG("SERVER_TERMINATE message\n\n");
            i_errno = IESERVERTERM;

	    /*
	     * Temporarily be in DISPLAY_RESULTS phase so we can get
	     * ending summary statistics.
	     */
	    signed char oldstate = test->state;
	    cpu_util(test->cpu_util);
	    test->state = DISPLAY_RESULTS;
	    test->reporter_callback(test);
	    test->state = oldstate;
            return -1;

        case ACCESS_DENIED:
		MY_DEBUG("ACCESS_DENIED message\n\n");
            i_errno = IEACCESSDENIED;
            return -1;

        case SERVER_ERROR:
		MY_DEBUG("SERVER_ERROR message\n\n");
            if (Nread(test->ctrl_sck, (char*) &err, sizeof(err), Ptcp) < 0) {
                i_errno = IECTRLREAD;
                return -1;
            }
	    i_errno = ntohl(err);
            if (Nread(test->ctrl_sck, (char*) &err, sizeof(err), Ptcp) < 0) {
                i_errno = IECTRLREAD;
                return -1;
            }
            errno = ntohl(err);
            return -1;

        default:
            i_errno = IEMESSAGE;
            return -1;
    }

    return 0;
}



/* iperf_connect -- client to server connection function */
int
iperf_connect(struct iperf_test *test)
{
    FD_ZERO(&test->read_set);
    FD_ZERO(&test->write_set);

    make_cookie(test->cookie);

	/*新建一个control test的通道，用于传输一些控制信息，可以看到使用的是 TCP 协议。
	即在用iperf3测试udp时，也会有一条tcp流用于传输control信息。
	*/
    /* Create and connect the control channel */
    if (test->ctrl_sck < 0) {
	// Create the control channel using an ephemeral port
	test->ctrl_sck = netdial(test->settings->domain, Ptcp, test->bind_address, 0, test->server_hostname, test->server_port, test->settings->connect_timeout);

	MY_DEBUG("create a control channel(TCP stream): bind_address:%s, server_hostname:%s, server_port:%d\n",
		test->bind_address, test->server_hostname, test->server_port);
    }
    
    if (test->ctrl_sck < 0) {
        i_errno = IECONNECT;
        return -1;
    }

	/* 发送control message -- uuid */
    if (Nwrite(test->ctrl_sck, test->cookie, COOKIE_SIZE, Ptcp) < 0) {
        i_errno = IESENDCOOKIE;
        return -1;
    }

    FD_SET(test->ctrl_sck, &test->read_set);
    if (test->ctrl_sck > test->max_fd) test->max_fd = test->ctrl_sck;

    int opt;
    socklen_t len;

	/* 获取tcp MSS */
    len = sizeof(opt);
    if (getsockopt(test->ctrl_sck, IPPROTO_TCP, TCP_MAXSEG, &opt, &len) < 0) {
        test->ctrl_sck_mss = 0;
    }
    else {
        if (opt > 0 && opt <= MAX_UDP_BLOCKSIZE) {
            test->ctrl_sck_mss = opt;
        }
        else {
            char str[128];
            snprintf(str, sizeof(str),
                     "Ignoring nonsense TCP MSS %d", opt);
            warning(str);

            test->ctrl_sck_mss = 0;
        }
    }

    if (test->verbose) {
	printf("Control connection MSS %d\n", test->ctrl_sck_mss);
    }

    /*
     * If we're doing a UDP test and the block size wasn't explicitly
     * set, then use the known MSS of the control connection to pick
     * an appropriate default.  If we weren't able to get the
     * MSS for some reason, then default to something that should
     * work on non-jumbo-frame Ethernet networks.  The goal is to
     * pick a reasonable default that is large but should get from
     * sender to receiver without any IP fragmentation.
     *
     * We assume that the control connection is routed the same as the
     * data packets (thus has the same PMTU).  Also in the case of
     * --reverse tests, we assume that the MTU is the same in both
     * directions.  Note that even if the algorithm guesses wrong,
     * the user always has the option to override.
     */
    if (test->protocol->id == Pudp) {
	if (test->settings->blksize == 0) {
	    if (test->ctrl_sck_mss) {
		test->settings->blksize = test->ctrl_sck_mss;
	    }
	    else {
		test->settings->blksize = DEFAULT_UDP_BLKSIZE;
	    }

	    MY_DEBUG("settings->blksize=%d; ctrl_sck_mss=%d, udp default blksize=%d\n",
	    test->settings->blksize, test->ctrl_sck_mss, DEFAULT_UDP_BLKSIZE);

	    if (test->verbose) {
		printf("Setting UDP block size to %d\n", test->settings->blksize);
	    }
	}

	/*
	 * Regardless of whether explicitly or implicitly set, if the
	 * block size is larger than the MSS, print a warning.
	 */
	if (test->ctrl_sck_mss > 0 &&
	    test->settings->blksize > test->ctrl_sck_mss) {
	    char str[128];
	    snprintf(str, sizeof(str),
		     "UDP block size %d exceeds TCP MSS %d, may result in fragmentation / drops", test->settings->blksize, test->ctrl_sck_mss);
	    warning(str);

	    MY_DEBUG("UDP block size %d exceeds TCP MSS %d, may result in fragmentation / drops\n", test->settings->blksize, test->ctrl_sck_mss);
	}
    }

    return 0;
}


int
iperf_client_end(struct iperf_test *test)
{
    struct iperf_stream *sp;

    /* Close all stream sockets */
    SLIST_FOREACH(sp, &test->streams, streams) {
        close(sp->socket);
    }

    /* show final summary */
    test->reporter_callback(test);

    if (iperf_set_send_state(test, IPERF_DONE) != 0)
        return -1;

    /* Close control socket */
    if (test->ctrl_sck)
        close(test->ctrl_sck);

    return 0;
}


int
iperf_run_client(struct iperf_test * test)
{
    int startup;
    int result = 0;
    fd_set read_set, write_set;
    struct iperf_time now;
    struct timeval* timeout = NULL;
    struct iperf_stream *sp;

	MY_DEBUG("start\n");

    if (test->logfile)
        if (iperf_open_logfile(test) < 0)
            return -1;

    if (test->affinity != -1)
	if (iperf_setaffinity(test, test->affinity) != 0)
	    return -1;

    if (test->json_output)
	if (iperf_json_start(test) < 0)
	    return -1;

    if (test->json_output) {
	cJSON_AddItemToObject(test->json_start, "version", cJSON_CreateString(version));
	cJSON_AddItemToObject(test->json_start, "system_info", cJSON_CreateString(get_system_info()));
    } else if (test->verbose) {
	iperf_printf(test, "%s\n", version);
	iperf_printf(test, "%s", "");
	iperf_printf(test, "%s\n", get_system_info());
	iflush(test);
    }

	/* 新建一条TCP流，用于传输控制信息。 */
    /* Start the client and connect to the server */
    if (iperf_connect(test) < 0)
        goto cleanup_and_fail;

    /* Begin calculating CPU utilization */
    cpu_util(NULL);

	/* 开始进行测试 */
    MY_DEBUG("**** client start run test ****\n\n");
    startup = 1;
    while (test->state != IPERF_DONE) {
    	/* 初始化该client要监控的socket fd。 */
	memcpy(&read_set, &test->read_set, sizeof(fd_set));
	memcpy(&write_set, &test->write_set, sizeof(fd_set));
	iperf_time_now(&now);
	timeout = tmr_timeout(&now);

	/* select()用于等待文件描述词状态的改变。
	 * 当检测的fd状态有变化或者超时时，则返回。
	 * 例如，当socket读缓冲区有数据时，则返回；当socket写缓冲区有空间时，则返回。
	 */
	result = select(test->max_fd + 1, &read_set, &write_set, NULL, timeout);
	if (result < 0 && errno != EINTR) {
  	    i_errno = IESELECT;
	    goto cleanup_and_fail;
	}

	/* 检测 control channle 是否有数据可读，会更近获取的server信息来进行动作，
	 * 例如：更新测试参数，新建测试流，测试结果等。
	 */
	if (result > 0) {
	    if (FD_ISSET(test->ctrl_sck, &read_set)) {
 	        if (iperf_handle_message_client(test) < 0) {
		    goto cleanup_and_fail;
		}
		FD_CLR(test->ctrl_sck, &read_set);
	    }
	}

	/* 进行发包uplink或者收包downlink测试 */
	if (test->state == TEST_RUNNING) {

		/* 只在第一次测试时执行，设置socket的blocking模式。 */
	    /* Is this our first time really running? */
	    if (startup) {
	        startup = 0;

		/* 对应非udp，则设置的是none blocking模式。
		 * 注意：
		 * 无论是TCP还是UDP，默认情况下创建的都是阻塞模式（blocking）的套接字
		 */
		// Set non-blocking for non-UDP tests
		if (test->protocol->id != Pudp) {
		    SLIST_FOREACH(sp, &test->streams, streams) {
			setnonblocking(sp->socket, 1);
		    }
		}
	    }

		MY_DEBUG("one round test start!\n");
		/* 双向模式 */
	    if (test->mode == BIDIRECTIONAL)
	    {
                if (iperf_send(test, &write_set) < 0)
                    goto cleanup_and_fail;
                if (iperf_recv(test, &read_set) < 0)
                    goto cleanup_and_fail;

		/* uplink 发送 */
	    } else if (test->mode == SENDER) {
                // Regular mode. Client sends.
                if (iperf_send(test, &write_set) < 0)
                    goto cleanup_and_fail;

		/* downlink 接收 */
	    } else {
                // Reverse mode. Client receives.
                if (iperf_recv(test, &read_set) < 0)
                    goto cleanup_and_fail;
	    }
		MY_DEBUG("one round test done!\n\n");

		/* 计算timer是否到期， 如果到期，就执行相关的timer函数。例如，统计和显示的timer*/
            /* Run the timers. */
            iperf_time_now(&now);

		/* 检查timer是否到期，如果到期则执行timer handler，并根据timer属性，看是否重新添加到timer list中。*/
            tmr_run(&now);

		/* 检查测试是否完成：
		 * 以测试时间(-t) 或者 测试传输的数据量(-n) 或者 测试传输的block(-k) 作为结束条件。
		 */
	    /* Is the test done yet? */
	    if ((!test->omitting) &&
	        ((test->duration != 0 && test->done) ||
	         (test->settings->bytes != 0 && test->bytes_sent >= test->settings->bytes) ||
	         (test->settings->blocks != 0 && test->blocks_sent >= test->settings->blocks))) {

		MY_DEBUG("**** client end run test ****\n\n");

		// Unset non-blocking for non-UDP tests
		if (test->protocol->id != Pudp) {
		    SLIST_FOREACH(sp, &test->streams, streams) {
			setnonblocking(sp->socket, 0);
		    }
		}

		/* Yes, done!  Send TEST_END. */
		test->done = 1;
		cpu_util(test->cpu_util);
		test->stats_callback(test);
		if (iperf_set_send_state(test, TEST_END) != 0)
                    goto cleanup_and_fail;
	    }

	/* 当client为receive模式下，并且测试结束，也要继续接收数据。*/
	}
	// If we're in reverse mode, continue draining the data
	// connection(s) even if test is over.  This prevents a
	// deadlock where the server side fills up its pipe(s)
	// and gets blocked, so it can't receive state changes
	// from the client side.
	else if (test->mode == RECEIVER && test->state == TEST_END) {
	    if (iperf_recv(test, &read_set) < 0)
		goto cleanup_and_fail;
	}
    }

    if (test->json_output) {
	if (iperf_json_finish(test) < 0)
	    return -1;
    } else {
	iperf_printf(test, "\n");
	iperf_printf(test, "%s", report_done);
    }

    iflush(test);

    return 0;

  cleanup_and_fail:
    iperf_client_end(test);
    if (test->json_output)
	iperf_json_finish(test);
    iflush(test);
    return -1;
}
