/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <getopt.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_mbuf_dyn.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_cycles.h>


#define RX_RING_SIZE (1 << 14)
#define TX_RING_SIZE (1 << 14)

#define NUM_MBUFS (1 << 16)
#define BURST_SIZE (1 << 6)

#define QUEUE_SIZE (1 << 13)

#define MBUF_CACHE_SIZE 256

/* >8 End of launching function on lcore. */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
    uint16_t nb_queue_pairs = 1;
    uint16_t rx_rings = nb_queue_pairs, tx_rings = nb_queue_pairs;
    uint16_t nb_rxd = RX_RING_SIZE;
    uint16_t nb_txd = TX_RING_SIZE;
    uint16_t rx_queue_size = QUEUE_SIZE;
    uint16_t tx_queue_size = QUEUE_SIZE;
    int retval;
    uint16_t q;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_rxconf rxconf;
    struct rte_eth_txconf txconf;
    struct rte_eth_conf port_conf = {
        .rxmode = {
		    .mq_mode = RTE_ETH_MQ_RX_RSS,
        },
        .rx_adv_conf = {
            .rss_conf = {
                .rss_key = NULL,
                .rss_hf = RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_TCP,
            },
        },
        .txmode = {
            .mq_mode = RTE_ETH_MQ_TX_NONE,
        },
    };

    if (!rte_eth_dev_is_valid_port(port))
        return -1;

    rte_eth_promiscuous_enable(port);

    retval = rte_eth_dev_info_get(port, &dev_info);
    if (retval != 0)
    {
        printf("Error during getting device (port %u) info: %s\n",
               port, strerror(-retval));

        return retval;
    }

    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
    if (retval != 0)
        return retval;
    rxconf = dev_info.default_rxconf;

    for (q = 0; q < rx_rings; q++)
    {
        retval = rte_eth_rx_queue_setup(port, q, rx_queue_size,
                                        rte_eth_dev_socket_id(port), &rxconf, mbuf_pool);
        if (retval < 0)
            return retval;
    }

    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;
    for (q = 0; q < tx_rings; q++)
    {
        retval = rte_eth_tx_queue_setup(port, q, tx_queue_size,
                                        rte_eth_dev_socket_id(port), &txconf);
        if (retval < 0)
            return retval;
    }
    retval = rte_eth_dev_start(port);
    if (retval < 0)
    {
        return retval;
    }
    return 0;
}

struct worker_args
{
    struct rte_mempool *mbuf_pool;
};

static int
lcore_main(void *args)
{
    struct worker_args *w_args = (struct worker_args *)args;
    struct rte_mempool *mbuf_pool = w_args->mbuf_pool;  

    uint16_t port;
    uint16_t ret;

    double sample[5];

    RTE_ETH_FOREACH_DEV(port)
    if (rte_eth_dev_socket_id(port) >= 0 &&
        rte_eth_dev_socket_id(port) !=
            (int)rte_socket_id())
        printf("WARNING, port %u is on remote NUMA node to "
               "polling thread.\n\tPerformance will "
               "not be optimal.\n",
               port);

    printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
           rte_lcore_id());


    uint16_t pkt_count = 0;
    uint16_t queue_id =  rte_lcore_id() - 1;


    for (;;)
    {
        // port=1;
        RTE_ETH_FOREACH_DEV(port)
        {
            struct rte_mbuf *bufs[BURST_SIZE];
            
            uint16_t nb_rx = rte_eth_rx_burst(port, queue_id,
                                              bufs, BURST_SIZE);

            // break;
            if (nb_rx > 0)
            {
                // printf("Ali \n");
                // uint64_t timestamp = rte_get_tsc_cycles();
                // uint64_t tsc_hz = rte_get_tsc_hz();
                // double timestamp_us = (double)timestamp / tsc_hz * 1e6;

                u_int16_t ethernet_type;
                for (int i = 0; i < nb_rx; i++)
                {
                    pkt_count +=1;

                }
                if (unlikely(nb_rx == 0))
                    continue;

                const uint16_t nb_tx = rte_eth_tx_burst(port, queue_id,
                                                        bufs, nb_rx);

                if (unlikely(nb_tx < nb_rx))
                {
                    uint16_t buf;

                    for (buf = nb_tx; buf < nb_rx; buf++)
                        rte_pktmbuf_free(bufs[buf]); 
                }

            }
        }
    }

    return 0;
}


static void close_ports(void);
static void close_ports(void)
{
    uint16_t portid;
    int ret;
    uint16_t nb_ports;
    nb_ports = rte_eth_dev_count_avail();
    for (portid = 0; portid < nb_ports; portid++)
    {
        printf("Closing port %d...", portid);
        ret = rte_eth_dev_stop(portid);
        if (ret != 0)
            rte_exit(EXIT_FAILURE, "rte_eth_dev_stop: err=%s, port=%u\n",
                     strerror(-ret), portid);
        rte_eth_dev_close(portid);
        printf(" Done\n");
    }
}

/* Initialization of Environment Abstraction Layer (EAL). 8< */
int main(int argc, char **argv)
{
    struct rte_mempool *mbuf_pool;
    uint16_t nb_ports;
    uint16_t portid;
    unsigned lcore_id;
    int ret;

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_panic("Cannot init EAL\n");

    argc -= ret;
    argv += ret;

    nb_ports = rte_eth_dev_count_avail();

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
                                        NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
                                        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    RTE_ETH_FOREACH_DEV(portid)
    if (port_init(portid, mbuf_pool) != 0)
    {
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n",
                 portid);
    }
    else{
        printf("port %u initialized\n",portid);
    };


    struct worker_args arguments = {
        .mbuf_pool = mbuf_pool
    };
    

    RTE_LCORE_FOREACH_WORKER(lcore_id)
    {
        rte_eal_remote_launch(lcore_main, &arguments, lcore_id);
    }


    rte_eal_mp_wait_lcore();

    close_ports();

    /* clean up the EAL */
    rte_eal_cleanup();

    return 0;
}