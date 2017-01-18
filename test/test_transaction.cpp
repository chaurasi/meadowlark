//************************************************
// Transaction unit tests
//************************************************
#include "radixtree/status.h"

#include "gtest/gtest.h"
#include "radixtree/Transaction.hpp"

#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include <pthread.h>

using namespace radixtree;

typedef Transaction::tid_t tid_t;
typedef Transaction::indexHandle indexHandle;

static const unsigned TEST_SIZE = 3;

static const unsigned BUFFER_SIZE = 1024;

static const unsigned MIN_STR_LEN = 2;
static const unsigned MAX_STR_LEN = 20;

static const char alphanum[] =
    "0123456789"
    "!@#$%^&*"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";

static const int stringLength = sizeof(alphanum) - 1;

typedef struct {
    unsigned i;
    std::vector<std::string> *keys;
} thread_arg;

class TransactionUnitTest : public ::testing::Test {
public:
    //*******************************************************
    // Random string generator
    inline char getRandomChar () {
	return alphanum[rand() % stringLength];
    }

    inline std::string getRandomString () {
	std::string str;
	unsigned len = MIN_STR_LEN + rand() % (MAX_STR_LEN - MIN_STR_LEN);
	for(unsigned i = 0; i < len; i++) {
	    str += getRandomChar();
	}
	return str;
    }
    //*******************************************************
    
    virtual void SetUp () {
	init_log(SeverityLevel::off, "");
	//srand((unsigned)time(NULL));
	srand(0); // fix seed for debugging
	//for (unsigned i = 0; i < TEST_SIZE; i++)
	//keys.push_back(getRandomString());

	TransactionManager *tmgr = TransactionManager::getInstance();
	tmgr->Reset();
    }

    inline void init (unsigned size) {
	for (unsigned i = 0; i < size; i++)
	    keys.push_back(getRandomString());
    }
    /*
    inline void load () {
	for (unsigned i = 0; i < TEST_SIZE; i++)
	    index->insert(keys[i].c_str(), (int)keys[i].length(),
			  keys[i].c_str(), (int)keys[i].length());
    }
    */
    inline void sortKeys () {
	std::sort(keys.begin(), keys.end());
    }


public:
    std::vector<std::string> keys;
};

//*************************************************************
// Single-Thread Tests
//*************************************************************

TEST_F(TransactionUnitTest, CreateIndexTest) {
    init(TEST_SIZE);
    
    tid_t tid;
    bool committed;
    Transaction *txn = new Transaction();
    txn->startTxn(tid);
    EXPECT_EQ(tid, (unsigned)1);

    std::string indexName = keys[0];
    EXPECT_TRUE(txn->createIndex(tid, indexName).ok());
    EXPECT_FALSE(txn->createIndex(tid, indexName).ok());

    indexName = keys[1];
    EXPECT_TRUE(txn->createIndex(tid, indexName).ok());
    EXPECT_FALSE(txn->createIndex(tid, indexName).ok());

    indexName = keys[2];
    EXPECT_TRUE(txn->createIndex(tid, indexName).ok());
    EXPECT_FALSE(txn->createIndex(tid, indexName).ok());

    txn->commitTxn(committed, tid);
}

TEST_F(TransactionUnitTest, CreateIndexTest2) {
    init(TEST_SIZE);
    
    tid_t tid;
    bool committed;
    Transaction *txn = new Transaction();
    txn->startTxn(tid);
    EXPECT_EQ(tid, 1);

    std::string indexName = keys[0];
    EXPECT_TRUE(txn->createIndex(tid, indexName).ok());
    EXPECT_FALSE(txn->createIndex(tid, indexName).ok());

    indexName = keys[1];
    EXPECT_TRUE(txn->createIndex(tid, indexName).ok());
    EXPECT_FALSE(txn->createIndex(tid, indexName).ok());

    indexName = keys[2];
    EXPECT_TRUE(txn->createIndex(tid, indexName).ok());
    EXPECT_FALSE(txn->createIndex(tid, indexName).ok());

    txn->commitTxn(committed, tid);
}


TEST_F(TransactionUnitTest, TxnTest) {
    init(TEST_SIZE);
    
    tid_t tid;
    indexHandle ih;
    bool committed;
    int i = 0;

    char keyBuf[BUFFER_SIZE];
    char valueBuf[BUFFER_SIZE];
    int keyBufLen = BUFFER_SIZE;
    int valueBufLen = BUFFER_SIZE;

    //insert TXN
    Transaction *txn = new Transaction();
    EXPECT_TRUE(txn->startTxn(tid).ok());
    EXPECT_EQ(tid, 1);
    
    std::string indexName = keys[i]; i++;
    EXPECT_TRUE(txn->createIndex(tid, indexName).ok());
    
    EXPECT_TRUE(txn->openIndex(ih, tid, indexName, Transaction::idxAccessType::INDEX_READ_WRITE).ok());

    std::string key = keys[i]; i++;
    EXPECT_TRUE(txn->insertIndexItem(ih, tid, key.c_str(), (int)key.length(), key.c_str(), (int)key.length()).ok());

    EXPECT_TRUE(txn->scanIndexItem(keyBuf, keyBufLen, valueBuf, valueBufLen, ih, tid, key.c_str(), (int)key.length(), true, key.c_str(), (int)key.length(), true).ok());
	
    EXPECT_EQ(keyBufLen, (int)key.length());
    
    EXPECT_TRUE(txn->commitTxn(committed, tid).ok());

    //read TXN
    txn = new Transaction();
    EXPECT_TRUE(txn->startTxn(tid).ok());
    EXPECT_EQ(tid, 2);

    keyBufLen = BUFFER_SIZE;
    valueBufLen = BUFFER_SIZE;
    
    EXPECT_TRUE(txn->openIndex(ih, tid, indexName, Transaction::idxAccessType::INDEX_READ_WRITE).ok());
    EXPECT_TRUE(txn->scanIndexItem(keyBuf, keyBufLen, valueBuf, valueBufLen, ih, tid, key.c_str(), (int)key.length(), true, key.c_str(), (int)key.length(), true).ok());
	
    EXPECT_EQ(keyBufLen, (int)key.length());
	
    EXPECT_TRUE(txn->commitTxn(committed, tid).ok());
}

//*************************************************************
// Multi-Thread Tests
//*************************************************************

void txnInit (void* arg) {
    thread_arg *targ = (thread_arg*)arg;
    unsigned i = targ->i;
    std::vector<std::string> keys = *(targ->keys);

    tid_t tid;
    indexHandle ih;
    bool committed;

    std::string indexName1 = keys[i]; i++;
    std::string key1 = keys[i]; i++;
    std::string key2 = keys[i]; i++;
    std::string indexName2 = keys[i]; i++;
    std::string key3 = keys[i]; i++;
    std::string key4 = keys[i]; i++;
    
    Transaction *txn = new Transaction();
    txn->startTxn(tid);

    std::cout << "*****tid = " << tid << "\n";
    
    txn->createIndex(tid, indexName1);
    
    txn->openIndex(ih, tid, indexName1, Transaction::idxAccessType::INDEX_READ_WRITE);
    txn->insertIndexItem(ih, tid, key1.c_str(), (int)key1.length(), key1.c_str(), (int)key1.length());
    txn->insertIndexItem(ih, tid, key2.c_str(), (int)key2.length(), key2.c_str(), (int)key2.length());

    txn->createIndex(tid, indexName2);

    txn->openIndex(ih, tid, indexName2, Transaction::idxAccessType::INDEX_READ_WRITE);
    txn->insertIndexItem(ih, tid, key3.c_str(), (int)key3.length(), key3.c_str(), (int)key3.length());
    txn->insertIndexItem(ih, tid, key4.c_str(), (int)key4.length(), key4.c_str(), (int)key4.length());
    
    txn->commitTxn(committed, tid);
}


void* txnWrite (void* arg) {
    thread_arg *targ = (thread_arg*)arg;
    unsigned i = targ->i;
    std::vector<std::string> keys = *(targ->keys);

    tid_t tid;
    indexHandle ih;
    bool committed;

    std::string indexName1 = keys[i]; i++;
    std::string key1 = keys[i]; i++;
    std::string key2 = keys[i]; i++;
    std::string indexName2 = keys[i]; i++;
    std::string key3 = keys[i]; i++;
    std::string key4 = keys[i]; i++;
    
    Transaction *txn = new Transaction();
    txn->startTxn(tid);

    std::cout << "*****tid = " << tid << "\n";
    
    txn->createIndex(tid, indexName1);

    txn->openIndex(ih, tid, indexName1, Transaction::idxAccessType::INDEX_READ_WRITE);
    txn->insertIndexItem(ih, tid, key1.c_str(), (int)key1.length(), key1.c_str(), (int)key1.length());
    txn->insertIndexItem(ih, tid, key2.c_str(), (int)key2.length(), key2.c_str(), (int)key2.length());

    txn->createIndex(tid, indexName2);

    txn->openIndex(ih, tid, indexName2, Transaction::idxAccessType::INDEX_READ_WRITE);
    txn->insertIndexItem(ih, tid, key3.c_str(), (int)key3.length(), key3.c_str(), (int)key3.length());
    txn->insertIndexItem(ih, tid, key4.c_str(), (int)key4.length(), key4.c_str(), (int)key4.length());
    
    txn->commitTxn(committed, tid);

    pthread_exit(NULL);
}

void* txnRead (void* arg) {
    thread_arg *targ = (thread_arg*)arg;
    unsigned i = targ->i;
    std::vector<std::string> keys = *(targ->keys);

    tid_t tid;
    indexHandle ih;
    bool committed;
	
    std::string indexName1 = keys[i]; i++;
    std::string key1 = keys[i]; i++;
    std::string key2 = keys[i]; i++;
    std::string indexName2 = keys[i]; i++;
    std::string key3 = keys[i]; i++;
    std::string key4 = keys[i]; i++;
    
    Transaction *txn = new Transaction();
    txn->startTxn(tid);

    std::cout << "*****tid = " << tid << "\n";

    char keyBuf[BUFFER_SIZE];
    char valueBuf[BUFFER_SIZE];
    int keyBufLen = BUFFER_SIZE;
    int valueBufLen = BUFFER_SIZE;

    txn->openIndex(ih, tid, indexName1, Transaction::idxAccessType::INDEX_READ_WRITE);

    keyBufLen = BUFFER_SIZE;
    valueBufLen = BUFFER_SIZE;
    txn->scanIndexItem(keyBuf, keyBufLen, valueBuf, valueBufLen, ih, tid, key1.c_str(), (int)key1.length(), true, key1.c_str(), (int)key1.length(), true);

    EXPECT_EQ(valueBufLen, (int)key1.length());

    keyBufLen = BUFFER_SIZE;
    valueBufLen = BUFFER_SIZE;
    txn->scanIndexItem(keyBuf, keyBufLen, valueBuf, valueBufLen, ih, tid, key2.c_str(), (int)key2.length(), true, key2.c_str(), (int)key2.length(), true);

    EXPECT_EQ(valueBufLen, (int)key2.length());

    txn->openIndex(ih, tid, indexName2, Transaction::idxAccessType::INDEX_READ_WRITE);
    keyBufLen = BUFFER_SIZE;
    valueBufLen = BUFFER_SIZE;
    txn->scanIndexItem(keyBuf, keyBufLen, valueBuf, valueBufLen, ih, tid, key3.c_str(), (int)key3.length(), true, key3.c_str(), (int)key3.length(), true);

    EXPECT_EQ(valueBufLen, (int)key3.length());

    keyBufLen = BUFFER_SIZE;
    valueBufLen = BUFFER_SIZE;
    txn->scanIndexItem(keyBuf, keyBufLen, valueBuf, valueBufLen, ih, tid, key4.c_str(), (int)key4.length(), true, key4.c_str(), (int)key4.length(), true);

    EXPECT_EQ(valueBufLen, (int)key4.length());
	
    txn->commitTxn(committed, tid);

    pthread_exit(NULL);
}

TEST_F(TransactionUnitTest, MultiThreadTxnTest) {
    init(TEST_SIZE * 10);

    pthread_t threads[TEST_SIZE];
    thread_arg args[TEST_SIZE];
    void *status;

    for (unsigned i = 0; i < TEST_SIZE; i++) {
	args[i].i = i * 6;
	args[i].keys = &keys;
	int ret = pthread_create(&threads[i], NULL, txnWrite, (void*)&args[i]);
	ASSERT_EQ(0, ret);
    }

    for(unsigned i = 0; i < TEST_SIZE; i++)
    {
        int ret = pthread_join(threads[i], &status);
        ASSERT_EQ(0, ret);
    }

    for (unsigned i = 0; i < TEST_SIZE - 1; i++) {
	int ret = pthread_create(&threads[i], NULL, txnRead, (void*)&args[i]);
	ASSERT_EQ(0, ret);
    }

    for(unsigned i = 0; i < TEST_SIZE - 1; i++)
    {
        int ret = pthread_join(threads[i], &status);
        ASSERT_EQ(0, ret);
    }

}

/*
TEST_F(TransactionUnitTest, MultiProcessTxnTest) {

}
*/

int main (int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
