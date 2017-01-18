//************************************************
// RadixTree unit tests
//************************************************
#include "radixtree/status.h"

#include "gtest/gtest.h"
#include "radixtree/RadixTree.hpp"
#include "radixtree/Transaction.hpp"

#include <stdlib.h>
#include <time.h>
#include <algorithm>

#include "radixtree/log.h"

using namespace radixtree;

static const unsigned TEST_SIZE = 10000;
//static const unsigned TEST_SIZE = 2;
static const unsigned VALUES_PER_KEY = 5;

static const unsigned BUFFER_SIZE = 1024;

static const unsigned MIN_STR_LEN = 2;
static const unsigned MAX_STR_LEN = 20;

static const char alphanum[] =
    "0123456789"
    "!@#$%^&*"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";

static const int stringLength = sizeof(alphanum) - 1;

class RadixTreeUnitTest : public ::testing::Test {
  public:
    //*******************************************************
    // Random string generator
    inline char getRandomChar () {
      return alphanum[rand() % stringLength];
    }

    inline std::string getRandomString () {
      std::string str = "c:\\home\\Daniel";
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

      keyBufLen = BUFFER_SIZE;
      valueBufLen = BUFFER_SIZE;

      index = new RadixTree();
      indexMulti = new RadixTree(false);
    }

    virtual void TearDown () {
      delete index;
      delete indexMulti;
    }

    inline void init (unsigned size) {
      for (unsigned i = 0; i < size; i++)
        keys.push_back(getRandomString());
    }

    inline void load () {
      for (unsigned i = 0; i < TEST_SIZE; i++)
        index->insert(keys[i].c_str(), (int)keys[i].length(),
            keys[i].c_str(), (int)keys[i].length());
    }

    inline void loadMulti () {
      for (unsigned i = 0; i < VALUES_PER_KEY; i++)
        values.push_back(getRandomString());

      sortKeys();

      for (unsigned i = 0; i < TEST_SIZE; i++) {
        while (keys[i] == keys[i+1])
          i++;
       
        for (unsigned j = 0; j < VALUES_PER_KEY; j++)
          ASSERT_EQ(indexMulti->insert(keys[i].c_str(), (int)keys[i].length(),
              values[j].c_str(), (int)values[j].length()).ok(), true);
      }
    }

    inline void sortKeys () {
      std::sort(keys.begin(), keys.end());
    }

  public:

    RadixTree *index;
    RadixTree *indexMulti;
    std::vector<std::string> keys;
    std::vector<std::string> values;

    char keyBuf[BUFFER_SIZE];
    int keyBufLen;
    char valueBuf[BUFFER_SIZE];
    int valueBufLen;

};



TEST_F(RadixTreeUnitTest, EmptyTest) {
  EXPECT_TRUE(true);
}

TEST_F(RadixTreeUnitTest, InsertFindTest) {
  init(TEST_SIZE);
  load();

  //index->printRoot();

  RadixTree::Iter iter;
  for (unsigned i = 0; i < TEST_SIZE; i++) {
    keyBufLen = BUFFER_SIZE;
    valueBufLen = BUFFER_SIZE;

    EXPECT_EQ(index->scan(keyBuf, keyBufLen, valueBuf, valueBufLen, iter, keys[i].c_str(), (int)keys[i].length(), true, keys[i].c_str(), (int)keys[i].length(), true).ok(), true);

    EXPECT_EQ(valueBufLen, (int)keys[i].length());

    for (int j = 0; j < valueBufLen; j++)
      EXPECT_EQ(valueBuf[j], keys[i].c_str()[j]);

    EXPECT_EQ(keyBufLen, (int)keys[i].length());

    for (int j = 0; j < keyBufLen; j++)
      EXPECT_EQ(keyBuf[j], keys[i].c_str()[j]);
  }
}


TEST_F(RadixTreeUnitTest, LowerBoundInclusiveTest) {
  init(TEST_SIZE);
  load();

  RadixTree::Iter iter;
  for (unsigned i = 0; i < TEST_SIZE - 1; i++) {
    keyBufLen = BUFFER_SIZE;
    valueBufLen = BUFFER_SIZE;

    EXPECT_EQ(index->scan(keyBuf, keyBufLen, valueBuf, valueBufLen, iter, keys[i].c_str(), (int)keys[i].length(), true, Transaction::OPEN_BOUNDARY.c_str(), (int)Transaction::OPEN_BOUNDARY.length(), false).ok(), true);

    EXPECT_EQ(valueBufLen, (int)keys[i].length());

    for (int j = 0; j < valueBufLen; j++)
      EXPECT_EQ(valueBuf[j], keys[i].c_str()[j]);

    EXPECT_EQ(keyBufLen, (int)keys[i].length());

    for (int j = 0; j < keyBufLen; j++)
      EXPECT_EQ(keyBuf[j], keys[i].c_str()[j]);
  }
}


TEST_F(RadixTreeUnitTest, LowerBoundExclusiveTest) {
  init(TEST_SIZE);
  load();
  sortKeys();

  int s = 0; //same key offset
  RadixTree::Iter iter;
  for (unsigned i = 0; i < TEST_SIZE - 1; i++) {
    keyBufLen = BUFFER_SIZE;
    valueBufLen = BUFFER_SIZE;

    i += s;
    s = 0;

    while (keys[i] == keys[i+1+s])
      s++;

    EXPECT_EQ(index->scan(keyBuf, keyBufLen, valueBuf, valueBufLen, iter, keys[i].c_str(), (int)keys[i].length(), false, Transaction::OPEN_BOUNDARY.c_str(), (int)Transaction::OPEN_BOUNDARY.length(), false).ok(), true);

    EXPECT_EQ(valueBufLen, (int)keys[i+1+s].length());

    for (int j = 0; j < valueBufLen; j++)
      EXPECT_EQ(valueBuf[j], keys[i+1+s].c_str()[j]);

    EXPECT_EQ(keyBufLen, (int)keys[i+1+s].length());

    for (int j = 0; j < keyBufLen; j++)
      EXPECT_EQ(keyBuf[j], keys[i+1+s].c_str()[j]);
  }
}


TEST_F(RadixTreeUnitTest, GetNextTest) {
  init(TEST_SIZE);
  load();
  sortKeys();
  keys.push_back(getRandomString());

  //index->printRoot();
  
  RadixTree::Iter iter;
  //index->lowerBound(iter, "\0", 1, true);
  keyBufLen = BUFFER_SIZE;
  valueBufLen = BUFFER_SIZE;
  EXPECT_EQ(index->scan(keyBuf, keyBufLen, valueBuf, valueBufLen, iter, Transaction::OPEN_BOUNDARY.c_str(), (int)Transaction::OPEN_BOUNDARY.length(), false, Transaction::OPEN_BOUNDARY.c_str(), (int)Transaction::OPEN_BOUNDARY.length(), false).ok(), true);

  keyBufLen = BUFFER_SIZE;
  valueBufLen = BUFFER_SIZE;
  
  while (index->getNext(keyBuf, keyBufLen, valueBuf, valueBufLen, iter).ok()) {

      std::string key(keyBuf, keyBufLen);
      std::string val(valueBuf, valueBufLen);
      
      EXPECT_EQ(key, val);
      
      keyBufLen = BUFFER_SIZE;
      valueBufLen = BUFFER_SIZE;
    
  }
}


TEST_F(RadixTreeUnitTest, UpdateTest) {
  init(TEST_SIZE);
  load();
  sortKeys();

  RadixTree::Iter iter;
  int s = 0; //same key offset
  for (unsigned i = 0; i < TEST_SIZE - 1; i++) {
    i += s;
    s = 0;

    while (keys[i] == keys[i+1+s])
      s++;

    Status retCode = index->update(keys[i].c_str(), (unsigned)keys[i].length(), keys[i+1+s].c_str(), (unsigned)keys[i+1+s].length());
    EXPECT_EQ(retCode.ok(), true);
  }

  s = 0;
  for (unsigned i = 0; i < TEST_SIZE - 1; i++) {
    i += s;
    s = 0;

    while (keys[i] == keys[i+1+s])
      s++;

    EXPECT_EQ(index->find(iter, keys[i].c_str(), (unsigned)keys[i].length()), true);

    RadixTree::Value* val = index->getValue(iter);

    EXPECT_EQ(val->len, (uint32_t)keys[i+1+s].length());
    for (unsigned j = 0; j < val->len; j++)
      EXPECT_EQ(val->data[j], keys[i+1+s].c_str()[j]);
  }
}


TEST_F(RadixTreeUnitTest, DeleteTest) {
  init(TEST_SIZE);
  load();
  sortKeys();

  RadixTree::Iter iter;
  int s = 0; //same key offset
  for (unsigned i = 0; i < TEST_SIZE - 1; i++) {
    while (keys[i] == keys[i+1])
      i++;

    if (i % 2 == 0) {
      Status retCode = index->remove(keys[i].c_str(), (unsigned)keys[i].length());
      EXPECT_EQ(retCode.ok(), true);
    }
  }

  for (unsigned i = 0; i < TEST_SIZE - 1; i++) {
    while (keys[i] == keys[i+1+s])
      i++;

    if (i % 2 == 0)
      EXPECT_EQ(index->find(iter, keys[i].c_str(), (unsigned)keys[i].length()), false);
    else {
      EXPECT_EQ(index->find(iter, keys[i].c_str(), (unsigned)keys[i].length()), true);

      RadixTree::Value* val = index->getValue(iter);

      EXPECT_EQ(val->len, (uint32_t)keys[i].length());
      for (unsigned j = 0; j < val->len; j++)
        EXPECT_EQ(val->data[j], keys[i].c_str()[j]);
    }
  }
}



TEST_F(RadixTreeUnitTest, MultiInsertFindTest) {
  init(TEST_SIZE + 1);
  loadMulti();

  int s = 0; //same key offset
  RadixTree::Iter iter;
  for (unsigned i = 0; i < TEST_SIZE - 1; i++) {
    keyBufLen = BUFFER_SIZE;
    valueBufLen = BUFFER_SIZE;

    i += s;
    s = 0;

    while (keys[i] == keys[i+1+s])
      s++;

    EXPECT_EQ(indexMulti->scan(keyBuf, keyBufLen, valueBuf, valueBufLen, iter, keys[i].c_str(), (int)keys[i].length(), true, keys[i+1+s].c_str(), (int)keys[i+1+s].length(), true).ok(), true);

    EXPECT_EQ(valueBufLen, (int)values[0].length());

    std::string valueBufStr(valueBuf, valueBufLen);
    std::string valuesStr(values[0]);
    EXPECT_EQ(valueBufStr, valuesStr);

    EXPECT_EQ(keyBufLen, (int)keys[i].length());
    std::string keyBufStr(keyBuf, keyBufLen);
    std::string keysStr(keys[i]);
    EXPECT_EQ(keyBufStr, keysStr);

    for (unsigned k = 1; k < VALUES_PER_KEY; k++) {
      unsigned k_reversed = VALUES_PER_KEY - k;
      keyBufLen = BUFFER_SIZE;
      valueBufLen = BUFFER_SIZE;

      EXPECT_EQ(indexMulti->getNext(keyBuf, keyBufLen, valueBuf, valueBufLen, iter).ok(), true);

      EXPECT_EQ(valueBufLen, (int)values[k_reversed].length());

      std::string valueBufStr(valueBuf, valueBufLen);
      std::string valuesStr(values[k_reversed]);
      EXPECT_EQ(valueBufStr, valuesStr);

      EXPECT_EQ(keyBufLen, (int)keys[i].length());

      std::string keyBufStr(keyBuf, keyBufLen);
      std::string keysStr(keys[i]);
      EXPECT_EQ(keyBufStr, keysStr);
    }

    keyBufLen = BUFFER_SIZE;
    valueBufLen = BUFFER_SIZE;

    ASSERT_EQ(indexMulti->getNext(keyBuf, keyBufLen, valueBuf, valueBufLen, iter).ok(), true);


    EXPECT_EQ(keyBufLen, (int)keys[i+1+s].length());
    std::string keyBufStr2(keyBuf, keyBufLen);
    std::string keysStr2(keys[i+1+s]);
    EXPECT_EQ(keyBufStr2, keysStr2);
  }
}


TEST_F(RadixTreeUnitTest, MultiDeleteTest) {
  init(TEST_SIZE + 1);
  loadMulti();

  int s = 0; //same key offset
  RadixTree::Iter iter;
  for (unsigned i = 0; i < TEST_SIZE - 1; i++) {
    while (keys[i] == keys[i+1])
      i++;
    EXPECT_EQ(indexMulti->remove(keys[i].c_str(), (unsigned)keys[i].length(), values[VALUES_PER_KEY/2].c_str(), (unsigned)values[VALUES_PER_KEY/2].length()).ok(), true);
  }

  s = 0;

  for (unsigned i = 0; i < TEST_SIZE - 1; i++) {
    keyBufLen = BUFFER_SIZE;
    valueBufLen = BUFFER_SIZE;

    i += s;
    s = 0;

    while (keys[i] == keys[i+1+s])
      s++;

    EXPECT_EQ(indexMulti->scan(keyBuf, keyBufLen, valueBuf, valueBufLen, iter, keys[i].c_str(), (int)keys[i].length(), true, keys[i+1+s].c_str(), (int)keys[i+1+s].length(), true).ok(), true);

    EXPECT_EQ(valueBufLen, (int)values[0].length());

    for (int j = 0; j < valueBufLen; j++)
      EXPECT_EQ(valueBuf[j], values[0].c_str()[j]);

    EXPECT_EQ(keyBufLen, (int)keys[i].length());

    for (int j = 0; j < keyBufLen; j++)
      EXPECT_EQ(keyBuf[j], keys[i].c_str()[j]);

    for (unsigned k = 1; k < VALUES_PER_KEY; k++) {
      keyBufLen = BUFFER_SIZE;
      valueBufLen = BUFFER_SIZE;

      unsigned k_reversed = VALUES_PER_KEY - k;
      if (k_reversed != VALUES_PER_KEY/2) {
        EXPECT_EQ(indexMulti->getNext(keyBuf, keyBufLen, valueBuf, valueBufLen, iter).ok(), true);

        EXPECT_EQ(valueBufLen, (int)values[k_reversed].length());


        std::string valueBufStr(valueBuf, valueBufLen);
        std::string valuesStr(values[k_reversed]);
        //std::cout << "Comparing " << valueBufStr << " with " << valuesStr << std::endl;
        EXPECT_EQ(valueBufStr, valuesStr);

        EXPECT_EQ(keyBufLen, (int)keys[i].length());

        std::string keyBufStr(keyBuf, keyBufLen);
        std::string keysStr(keys[i]);
        EXPECT_EQ(keyBufStr, keysStr);
      }
    }

    keyBufLen = BUFFER_SIZE;
    valueBufLen = BUFFER_SIZE;

    ASSERT_EQ(indexMulti->getNext(keyBuf, keyBufLen, valueBuf, valueBufLen, iter).ok(), true);

    EXPECT_EQ(valueBufLen, (int)values[0].length());

    for (int j = 0; j < valueBufLen; j++)
      EXPECT_EQ(valueBuf[j], values[0].c_str()[j]);

    EXPECT_EQ(keyBufLen, (int)keys[i+1+s].length());

    for (int j = 0; j < keyBufLen; j++)
      EXPECT_EQ(keyBuf[j], keys[i+1+s].c_str()[j]);

  }
}


TEST_F(RadixTreeUnitTest, MultiDeleteTest2) {
  init(TEST_SIZE + 1);
  loadMulti();

  RadixTree::Iter iter;
  for (unsigned i = 0; i < TEST_SIZE - 1; i++) {
    while (keys[i] == keys[i+1])
      i++;

    if (i % 2 == 0)
      EXPECT_EQ(indexMulti->remove(keys[i].c_str(), (unsigned)keys[i].length()).ok(), true);
  }

  for (unsigned i = 0; i < TEST_SIZE - 1; i++) {
    keyBufLen = BUFFER_SIZE;
    valueBufLen = BUFFER_SIZE;

    while (keys[i] == keys[i+1])
      i++;

    if (i % 2 == 0)
      EXPECT_EQ(indexMulti->scan(keyBuf, keyBufLen, valueBuf, valueBufLen, iter, keys[i].c_str(), (int)keys[i].length(), true, keys[i].c_str(), (int)keys[i].length(), true).ok(), false);
    else {
      EXPECT_EQ(indexMulti->scan(keyBuf, keyBufLen, valueBuf, valueBufLen, iter, keys[i].c_str(), (int)keys[i].length(), true, keys[i].c_str(), (int)keys[i].length(), true).ok(), true);

      EXPECT_EQ(valueBufLen, (int)values[0].length());

      for (int j = 0; j < valueBufLen; j++)
        EXPECT_EQ(valueBuf[j], values[0].c_str()[j]);

      EXPECT_EQ(keyBufLen, (int)keys[i].length());

      for (int j = 0; j < keyBufLen; j++)
        EXPECT_EQ(keyBuf[j], keys[i].c_str()[j]);
    }
  }
}


int main (int argc, char** argv) {
  bool debug_console = false;
  int c;
  while ((c = getopt (argc, argv, "d")) != -1)
    switch (c) {
      case 'd':
        debug_console = true;
        break;
      default:
        continue;
    }
  if (debug_console)
    init_log(debug, "");
  else
    init_log(debug);

  ::testing::InitGoogleTest(&argc, argv);
  // remove existing files in SHELF_BASE_DIR-------------------------------------
  std::string cmd = std::string("exec rm -f ") + SHELF_BASE_DIR + "/" + SHELF_USER + "* > nul";
  system(cmd.c_str());
  //-----------------------------------------------------------------------------

  return RUN_ALL_TESTS();
}
