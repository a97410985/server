/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// test that an update calls back into the update function

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

DB_ENV *env;

int updates_called[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// the commands are: byte 1 is "nop" "add" or "del".  Byte 2 is the amount to add.
enum cmd { CNOP, CADD, CDEL };

static int update_fun(DB *UU(db),
                      const DBT *key,
                      const DBT *UU(old_val), const DBT *UU(extra),
                      void UU((*set_val)(const DBT *new_val,
                                         void *set_extra)),
                      void *UU(set_extra)) {
    unsigned int *k;
    assert(key->size == sizeof(*k));
    CAST_FROM_VOIDP(k, key->data);
    assert(updates_called[*k] == 0);
    updates_called[*k] = 1;
    return 0;
}

static void setup (void) {
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    { int chk_r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    env->set_errfile(env, stderr);
    env->set_update(env, update_fun);
    { int chk_r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
}

static void cleanup (void) {
    { int chk_r = env->close(env, 0); CKERR(chk_r); }
}

static void run_test(bool is_resetting) {
    DB *db;
    uint32_t update_flags = is_resetting ? DB_IS_RESETTING_OP : 0;
    for (unsigned int i = 0;
         i < (sizeof(updates_called) / sizeof(updates_called[0])); ++i) {
        updates_called[i] = 0;
    }

    {
        DB_TXN* txna = NULL;
        { int chk_r = env->txn_begin(env, NULL, &txna, 0); CKERR(chk_r); }

        { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
        { int chk_r = db->open(db, txna, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }

        {
            DBT key, val;
            unsigned int i;
            DBT *keyp = dbt_init(&key, &i, sizeof(i));
            DBT *valp = dbt_init(&val, "a", 2);
            for (i = 0; i < (sizeof(updates_called) / sizeof(updates_called[0])); ++i) {
                { int chk_r = db->put(db, txna, keyp, valp, 0); CKERR(chk_r); }
            }
        }

        { int chk_r = txna->commit(txna, 0); CKERR(chk_r); }
    }

    {
        DB_TXN *txnb = NULL;
        { int chk_r = env->txn_begin(env, NULL, &txnb, 0); CKERR(chk_r); }

        {
            DBT nullextra;
            DBT *nullextrap = dbt_init(&nullextra, NULL, 0);
            { int chk_r = db->update_broadcast(db, txnb, nullextrap, update_flags); CKERR(chk_r); }
        }

        { int chk_r = txnb->commit(txnb, 0); CKERR(chk_r); }
    }

    { int chk_r = db->close(db, 0); CKERR(chk_r); }

    for (unsigned int i = 0;
         i < (sizeof(updates_called) / sizeof(updates_called[0])); ++i) {
        assert(updates_called[i]);
    }

}

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    setup();
    run_test(true);
    run_test(false);
    cleanup();
    return 0;
}
