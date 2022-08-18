// https://docs.oracle.com/cd/E17076_05/html/articles/inmemory/C/index.html

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <db.h>
#include <stdint.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ipv4_log.h"

namespace xameleon {

    ipv4_log::ipv4_log()
    {
    }

    ipv4_log::~ipv4_log()
    {
    }

    int ipv4_log::list(int from, int count, ip_list_callback_t cb, void * obj)
    {
        int ret;
        DBC* dbcp;
        DBT key, data;

        /* Acquire a cursor for the database. */
        if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
            dbp->err(dbp, ret, "DB->cursor");
            return (1);
        }

        /* Re-initialize the key/data pair. */
        memset(&key, 0, sizeof(key));
        memset(&data, 0, sizeof(data));

        /* Walk through the database and print out the key/data pairs. */
        int idx = 0, i = 0;
        while ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0) {
            if (idx < from) {
                idx++;
                continue;
            }
            if (count && i >= count)
                break;
            ret = cb(obj, (ipv4_record_t*) data.data);
            i++;
            idx++;
        }
        if (ret != DB_NOTFOUND)
            dbp->err(dbp, ret, "DBcursor->get");


        /* Close the cursor. */
        if ((ret = dbcp->c_close(dbcp)) != 0) {
            dbp->err(dbp, ret, "DBcursor->close");
            return (1);
        }
        return ret;
    }


    int ipv4_log::sprint_ip(char* str, uint32_t ip)
    {
        return snprintf(str, 32, "%u.%u.%u.%u",
            ip & 0xff, (ip >> 8) & 0xff, (ip >> 16) & 0xff, (ip >> 24) & 0xff);
    }

    void dump_record(DBT rec, char * str)
    {
        printf("=========== %s ======>\n", str);
        const char* p = (const char*)rec.data;
        for (int i = 0; i < rec.size; i++) {
            printf("%02x ", p[i]);
        }
        printf("\n");
    }

    int ipv4_log::load_record(int32_t ip, ipv4_record_t* record)
    {
        int ret;
        DBT key, data;
//        char ip32[32];

        memset(&key, 0, sizeof(key));
        memset(&data, 0, sizeof(data));
        key.data = (void*)&ip;
        key.size = sizeof(int32_t);
        data.data = (void*)record;
        data.size = sizeof(ipv4_record_t);
        ret = dbp->get(dbp, NULL, &key, &data, 0);

        switch (ret) {
        case 0:
        {
            char first[20]; // , last[20];
            strftime(first, 20, "%Y-%m-%d %H:%M:%S", localtime(&(record->first_seen)));
            printf("First seen: %s\n", first);
            dump_record(data, "READ");
            memcpy(record, data.data, sizeof(ipv4_record_t));
        }

            break;

        case DB_NOTFOUND:
            break;

        default:
            printf("dbp->get(dbp, NULL, &key, &data, 0) = %d\n", ret);
            dbp->err(dbp, ret, "DB->get");
        }

        return ret;
    }

    int ipv4_log::save_record(ipv4_record_t* value)
    {
        int ret;
        DBT key, data;
        char ip32[32];

        memset(&key, 0, sizeof(key));
        memset(&data, 0, sizeof(data));
        key.data = (void*)&value->ip;
        key.size = sizeof(uint32_t);
        //data.data = (void*)value;
        //data.size = sizeof(ipv4_record_t);
        //ipv4_record_t * record = nullptr;
        //ret = dbp->get(dbp, NULL, &key, &data, 0);
        //switch (ret) {
        //case 0:
        //    record = (ipv4_record_t*) data.data;
        //    value->first_seen = record->first_seen;
        //    value->counters.total_request += record->counters.total_request;
        //    value->counters.bad_request += record->counters.bad_request;
        //    value->counters.not_found += record->counters.not_found;
        //    value->counters.http2https += record->counters.http2https;
        //    value->last_seen = time(NULL);
        //    break;
        //case DB_NOTFOUND:
        //    value->first_seen = time(NULL);
        //    value->last_seen = time(NULL);
        //    break;
        //default:
        //    printf("dbp->get(dbp, NULL, &key, &data, 0) = %d\n", ret);
        //    dbp->err(dbp, ret, "DB->get");
        //}

        data.data = (void*)value;
        data.size = sizeof(ipv4_record_t);
        dump_record(data, "WRITE");
        ret = dbp->put(dbp, NULL, &key, &data, 0);
        switch (ret) {
        case 0:
            break;
        case DB_KEYEXIST:
            sprint_ip(ip32, value->ip);
            printf("db: ip address %s already exist in database.\n", ip32);
            break;
        default:
            dbp->err(dbp, ret, "DB->put");
        }
        return ret;
    }

    int ipv4_log::show_database()
    {
        int ret;
        DBC* dbcp;
        DBT key, data;

        /* Acquire a cursor for the database. */
        if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
            dbp->err(dbp, ret, "DB->cursor");
            return (1);
        }


        /* Re-initialize the key/data pair. */
        memset(&key, 0, sizeof(key));
        memset(&data, 0, sizeof(data));


        /* Walk through the database and print out the key/data pairs. */
        int idx = 0;
        while ((ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT)) == 0) {
            //printf("%lu : %.*s\n",
            //    *(u_long*)key.data, (int)data.size, (char*)data.data);
            ipv4_record_t* value = (ipv4_record_t*)(data.data);
            char first[20], last[20];
            strftime(first, 20, "%Y-%m-%d %H:%M:%S", localtime(&value->first_seen));
            strftime(last, 20, "%d.%m.%Y %H:%M:%S", localtime(&value->last_seen));
            char ip32[32];
            sprint_ip(ip32, value->ip);
            fprintf(stdout, "%04u: %-16s %s   %s   %u\n", idx, ip32, first, last, value->counters.total_request);
            idx++;
        }
        if (ret != DB_NOTFOUND)
            dbp->err(dbp, ret, "DBcursor->get");


        /* Close the cursor. */
        if ((ret = dbcp->c_close(dbcp)) != 0) {
            dbp->err(dbp, ret, "DBcursor->close");
            return (1);
        }
        return ret;
    }


    int ipv4_log::test()
    {
        ipv4_record_t    rec;
        rec.ip = 80834787;
        rec.counters.total_request = 1;
        rec.counters.not_found = 1;

        save_record(&rec);
        rec.ip = 3333;
        save_record(&rec);
        rec.ip = 773333;
        save_record(&rec);
        return 0;
    }

    DB* ipv4_log::create_ip_database()
    {
        /* Initialize our handles */
        dbp = NULL;
        DB_MPOOLFILE* mpf = NULL;

        int ret, ret_t;
        const char* db_name = "in_mem_ipv4";
        u_int32_t open_flags;

        /* Create the environment */
        ret = db_env_create(&envp, 0);
        if (ret != 0) {
            fprintf(stderr, "Error creating environment handle: %s\n",
                db_strerror(ret));
            goto err;
        }

        open_flags =
            DB_CREATE |  /* Create the environment if it does not exist */
            DB_INIT_LOCK |  /* Initialize the locking subsystem */
            DB_INIT_LOG |  /* Initialize the logging subsystem */
            DB_INIT_MPOOL |  /* Initialize the memory pool (in-memory cache) */
            DB_INIT_TXN |
            DB_PRIVATE;      /* Region files are not backed by the filesystem.
                              * Instead, they are backed by heap memory.  */

                              /* Specify in-memory logging */
        ret = envp->log_set_config(envp, DB_LOG_IN_MEMORY, 1);
        if (ret != 0) {
            fprintf(stderr, "Error setting log subsystem to in-memory: %s\n",
                db_strerror(ret));
            goto err;
        }
        /*
         * Specify the size of the in-memory log buffer.
         */
        ret = envp->set_lg_bsize(envp, 10 * 1024 * 1024);
        if (ret != 0) {
            fprintf(stderr, "Error increasing the log buffer size: %s\n",
                db_strerror(ret));
            goto err;
        }

        /*
         * Specify the size of the in-memory cache.
         */
        ret = envp->set_cachesize(envp, 0, 10 * 1024 * 1024, 1);
        if (ret != 0) {
            fprintf(stderr, "Error increasing the cache size: %s\n",
                db_strerror(ret));
            goto err;
        }

        /*
         * Now actually open the environment. Notice that the environment home
         * directory is NULL. This is required for an in-memory only
         * application.
         */
        ret = envp->open(envp, NULL, open_flags, 0);
        if (ret != 0) {
            fprintf(stderr, "Error opening environment: %s\n",
                db_strerror(ret));
            goto err;
        }


        /* Initialize the DB handle */
        ret = db_create(&dbp, envp, 0);
        if (ret != 0) {
            envp->err(envp, ret,
                "Attempt to create db handle failed.");
            goto err;
        }


        /*
         * Set the database open flags. Autocommit is used because we are
         * transactional.
         */
        open_flags = DB_CREATE | DB_AUTO_COMMIT;
        ret = dbp->open(dbp,         /* Pointer to the database */
            NULL,        /* Txn pointer */
            NULL,        /* File name -- Must be NULL for inmemory! */
            db_name,     /* Logical db name */
            DB_BTREE,    /* Database type (using btree) */
            open_flags,  /* Open flags */
            0);          /* File mode. Using defaults */

        if (ret != 0) {
            envp->err(envp, ret,
                "Attempt to open db failed.");
            goto err;
        }

        /* Configure the cache file */
        mpf = dbp->get_mpf(dbp);
        ret = mpf->set_flags(mpf, DB_MPOOL_NOFILE, 1);

        if (ret == 0)
            return dbp;

        envp->err(envp, ret,
            "Attempt failed to configure for no backing of temp files.");

    err:
        /* Close our database handle, if it was opened. */
        if (dbp != NULL) {
            ret_t = dbp->close(dbp, 0);
            if (ret_t != 0) {
                fprintf(stderr, "%s database close failed.\n",
                    db_strerror(ret_t));
            }
        }
        return nullptr;
    }

    ipv4_log* get_ip_database()
    {
        static xameleon::ipv4_log   log;
        return &log;
    }
    int ipv4_log::release_ip_database()
    {
        int ret = 0;
        /* Close our environment, if it was opened. */
        if (envp != NULL) {
            ret = envp->close(envp, 0);
            if (ret != 0) {
                fprintf(stderr, "environment close failed: %s\n",
                    db_strerror(ret));
            }
        }
        return ret;
    }
}

#ifdef DEBUG_AND_TEST
int main(int argc, char* argv[], char* env[])
{
    int ret;
    static xameleon::ipv4_log   log;

    if (sizeof(time_t) < 8) {
        fprintf(stderr, "time_t is not 64 bits. Giving up.\n");
        exit(1);
    }

    if( log.create_ip_database() != nullptr) {

        do {
            log.test();
            log.show_database();
        } while (getchar() != 'q');

        ret = log.release_ip_database();
    }

    return ret;
}
#else
#endif
