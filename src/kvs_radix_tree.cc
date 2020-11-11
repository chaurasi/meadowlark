/*
 *  (c) Copyright 2016-2020 Hewlett Packard Enterprise Development Company LP.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  As an exception, the copyright holders of this Library grant you permission
 *  to (i) compile an Application with the Library, and (ii) distribute the
 *  Application containing code generated by the Library and added to the
 *  Application during this compilation process under terms of your choice,
 *  provided you also meet the terms and conditions of the Application license.
 *
 */

#include <stddef.h>
#include <stdint.h>
#include <iostream>
#include <string>
#include <string.h> // memset, memcpy
#include <utility> // pair

#include "nvmm/error_code.h"
#include "nvmm/global_ptr.h"
#include "nvmm/shelf_id.h"
#include "nvmm/memory_manager.h"
#include "nvmm/heap.h"
#include "nvmm/log.h"

#include "nvmm/fam.h"
#include "kvs_radix_tree.h"

namespace radixtree {

KVSRadixTree::KVSRadixTree(Gptr root, std::string base, std::string user, size_t heap_size, nvmm::PoolId heap_id, RadixTreeMetrics* metrics)
    : heap_id_(heap_id), heap_size_(heap_size),
      mmgr_(Mmgr::GetInstance()), emgr_(Emgr::GetInstance()), heap_(nullptr),
      tree_(nullptr), root_(root), metrics_(metrics) {
    int ret = Open();
    assert(ret==0);
}

KVSRadixTree::~KVSRadixTree() {
    int ret = Close();
    assert(ret==0);
}

void KVSRadixTree::Maintenance() {
    heap_->OfflineFree();
}

int KVSRadixTree::Open() {
    nvmm::ErrorCode ret;

    // find the heap
    heap_ = mmgr_->FindHeap(heap_id_);
    if(!heap_) {
        ret = mmgr_->CreateHeap(heap_id_, heap_size_);
        if(ret!=nvmm::NO_ERROR)
            return -1;
        heap_ = mmgr_->FindHeap(heap_id_);
    }
    assert(heap_);

    // open the heap
    ret = heap_->Open();
    if(ret!=nvmm::NO_ERROR) {
        delete heap_;
        return -1;
    }

    // create/open the radixtree
// #ifdef DEBUG
//     if(!root_)
//         std::cout << "create a new radix tree: ";
//     else
//         std::cout << "open an existing radix tree: ";
// #endif
    tree_ = new RadixTree(mmgr_, heap_, metrics_, root_);
    if (!tree_) {
        delete heap_;
        return -1;
    }
    root_ = tree_->get_root();
// #ifdef DEBUG
//     std::cout << (uint64_t)root_ << std::endl;
// #endif
    return 0;
}

int KVSRadixTree::Close() {
    nvmm::ErrorCode ret;

    // close the radix tree
    if (tree_) {
// #ifdef DEBUG
//         std::cout << "close the radix tree: " << root_;
// #endif
        delete tree_;
        tree_ = nullptr;
    }

    // close the heap
    if (heap_ && heap_->IsOpen()) {
        ret = heap_->Close();
        if(ret!=nvmm::NO_ERROR)
            return -1;
        delete heap_;
        heap_ = nullptr;
    }

    // delete all iters
    for (auto iter:iters_) {
        if (iter)
            delete iter;
    }
    return 0;
}


int KVSRadixTree::Put (char const *key, size_t const key_len,
                       char const *val, size_t const val_len) {
    //std::cout << "PUT" << " " << std::string(key, key_len) << " " << std::string(val, val_len) << std::endl;

    if (key_len > kMaxKeyLen)
        return -1;
    if (val_len > kMaxValLen)
        return -1;

    Eop op(emgr_);

    Gptr val_gptr = heap_->Alloc(op, val_len+sizeof(ValBuf));
    if (!val_gptr.IsValid())
        return -1;

    //std::cout << " allocated memory at " << val_gptr << std::endl;

    ValBuf *val_ptr = (ValBuf*)mmgr_->GlobalToLocal(val_gptr);

    val_ptr->size = val_len;
    memcpy((char*)val_ptr->val, (char const *)val, val_len);
    fam_persist(val_ptr, sizeof(ValBuf)+val_len);

    TagGptr old_value = tree_->put(key, key_len, val_gptr, UPDATE);
    if (old_value.IsValid()) {
// #ifdef DEBUG
//        std::cout << "  successfully updated "
//                  << std::string(key, key_len) << " = " << std::string(val, val_len);
//         ValBuf *val_ptr = (ValBuf*)mmgr_->GlobalToLocal(val_gptr);
//         std::cout << "  (old value " << std::string(val_ptr->val, val_ptr->size) << ")";

//         std::cout << std::endl;
//         std::cout << "  delayed free memory at " << val_gptr << std::endl;
// #endif
//        std::cout << "  delayed free memory at " << old_value << std::endl;
        heap_->Free(op, old_value.gptr());
    }
    else {
// #ifdef DEBUG
//         std::cout << "  successfully inserted "
//                   << std::string(key, key_len) << " = " << std::string(val, val_len) << std::endl;
// #endif
    }

    return 0;
}

int KVSRadixTree::Get (char const *key, size_t const key_len,
                       char *val, size_t &val_len) {
    //std::cout << "GET" << " " << std::string(key, key_len) << std::endl;

    if (key_len > kMaxKeyLen)
        return -1;

    Eop op(emgr_);

    TagGptr val_ptr = tree_->get(key, key_len);
    if (!val_ptr.IsValid()) {
        //std::cout << val_ptr.gptr() << std::endl;
        return -2;
    }

    ValBuf *val_p = (ValBuf*)mmgr_->GlobalToLocal(val_ptr.gptr());
    fam_invalidate(&val_p->size, sizeof(size_t));
    size_t val_size = val_p->size;
    if(val_len < val_size) {
        std::cout << "  val buffer is too small: " << val_len << " -> " << val_size << std::endl;
        val_len = val_size;
        return -1;
    }
    val_len = val_size;
    fam_invalidate(&val_p->val, val_len);
    fam_memcpy((char*)val, (char*)val_p->val, val_len);
// #ifdef DEBUG
//     std::cout << "  successfully fetched "
//               << std::string(key, key_len) << " -> " << std::string(val, val_len) << std::endl;
// #endif
    return 0;
}

int KVSRadixTree::Del (char const *key, size_t const key_len) {
    //std::cout << "DEL" << " " << std::string(key, key_len) << std::endl;
    if (key_len > kMaxKeyLen)
        return -1;

    Eop op(emgr_);

    TagGptr val_gptr = tree_->destroy(key, key_len);
    if (val_gptr.IsValid()) {
// #ifdef DEBUG
//         std::cout << "  successfully deleted " << std::string(key, key_len);
//         ValBuf *val_ptr = (ValBuf*)mmgr_->GlobalToLocal(val_gptr);
//         std::cout << " = " << std::string(val_ptr->val, val_ptr->size);

//         std::cout << std::endl;
//         std::cout << "  delayed free memory at " << val_gptr << std::endl;
// #endif
        heap_->Free(op, val_gptr.gptr());
        return 0;
    }
    else {
// #ifdef DEBUG
//         std::cout << "  not found: "
//                   << std::string(key, key_len) << std::endl;
// #endif
        return -2;
    }
}

int KVSRadixTree::Scan (
    int &iter_handle,
    char *key, size_t &key_len,
    char *val, size_t &val_len,
    char const *begin_key, size_t const begin_key_len,
    bool const begin_key_inclusive,
    char const *end_key, size_t const end_key_len,
    bool const end_key_inclusive) {

    if (begin_key_len > kMaxKeyLen || end_key_len > kMaxKeyLen)
        return -1;
    if (key_len > kMaxKeyLen)
        return -1;
    if (val_len > kMaxValLen)
        return -1;

    Eop op(emgr_);

    RadixTree::Iter *iter=new RadixTree::Iter();
    TagGptr val_gptr;
    int ret = tree_->scan(*iter,
                          key, key_len, val_gptr,
                          begin_key, begin_key_len, begin_key_inclusive,
                          end_key, end_key_len, end_key_inclusive);
    if (ret!=0)
        return -2; // no key in range

    // copy val
    ValBuf *val_ptr = (ValBuf*)mmgr_->GlobalToLocal(val_gptr.gptr());
    fam_invalidate(&val_ptr->size, sizeof(size_t));
    size_t val_size = val_ptr->size;
    if(val_len < val_size) {
        std::cout << "  val buffer is too small: " << val_len << " -> " << val_size << std::endl;
        val_len = val_size;
        return -1;
    }
    val_len = val_size;
    fam_invalidate(&val_ptr->val, val_len);
    fam_memcpy((char*)val, (char*)val_ptr->val, val_len);
// #ifdef DEBUG
    // std::cout << "  SCAN: successfully fetched "
    //           << std::string(key, key_len) << " -> "
    //           << std::string(val, val_len)
    //           << std::endl;
// #endif

    // assign iter handle
    std::lock_guard<std::mutex> lock(mutex_);
    iters_.push_back(iter);
    iter_handle = (int)(iters_.size()-1);
    return 0;
}

int KVSRadixTree::GetNext(int iter_handle,
                          char *key, size_t &key_len,
                          char *val, size_t &val_len) {
    if (key_len > kMaxKeyLen)
        return -1;
    if (val_len > kMaxValLen)
        return -1;
    if (iter_handle <0 || iter_handle >= (int)iters_.size())
        return -1;

    TagGptr val_gptr;

    RadixTree::Iter *iter;
    {
        //std::lock_guard<std::mutex> lock(mutex_);
        iter = iters_[iter_handle];
    }
    int ret = tree_->get_next(*iter,
                              key, key_len, val_gptr);
    if (ret!=0)
        return -2; // no next key

    // copy val
    ValBuf *val_ptr = (ValBuf*)mmgr_->GlobalToLocal(val_gptr.gptr());
    fam_invalidate(&val_ptr->size, sizeof(size_t));
    size_t val_size = val_ptr->size;
    if(val_len < val_size) {
        std::cout << "  val buffer is too small: " << val_len << " -> " << val_size << std::endl;
        val_len = val_size;
        return -1;
    }
    val_len = val_size;
    fam_invalidate(&val_ptr->val, val_len);
    fam_memcpy((char*)val, (char*)val_ptr->val, val_len);
// #ifdef DEBUG
//     std::cout << "  GET_NEXT: successfully fetched "
//               << std::string(key, key_len) << " -> "
//               << std::string(val, val_len)
//               << std::endl;
// #endif
    return 0;
}


/*
  for consistent DRAM caching
*/
int KVSRadixTree::Put (char const *key, size_t const key_len,
                       char const *val, size_t const val_len,
                       Gptr &key_ptr, TagGptr &val_ptr) {
    //std::cout << "PUT" << " " << std::string(key, key_len) << " " << std::string(val, val_len) << std::endl;

    if (key_len > kMaxKeyLen)
        return -1;
    if (val_len > kMaxValLen)
        return -1;

    Eop op(emgr_);

    Gptr val_gptr = heap_->Alloc(op, val_len+sizeof(ValBuf));
    if (!val_gptr.IsValid())
        return -1;

    ValBuf *val_p = (ValBuf*)mmgr_->GlobalToLocal(val_gptr);

    val_p->size = val_len;
    memcpy((char*)val_p->val, (char const *)val, val_len);
    fam_persist(val_p, sizeof(ValBuf)+val_len);

    TagGptr old_value;
    std::pair<Gptr, TagGptr> kv_ptr = tree_->putC(key, key_len, val_gptr, old_value);
    assert(kv_ptr.first.IsValid());
    if (old_value.IsValid()) {
        heap_->Free(op, old_value.gptr());
    }

    key_ptr = kv_ptr.first;
    val_ptr = kv_ptr.second;

    return 0;
}

int KVSRadixTree::Put (Gptr const key_ptr, TagGptr &val_ptr,
                       char const *val, size_t const val_len) {
    //std::cout << "PUT" << " " << std::string(key, key_len) << " " << std::string(val, val_len) << std::endl;

    if (val_len > kMaxValLen)
        return -1;

    Eop op(emgr_);

    Gptr val_gptr = heap_->Alloc(op, val_len+sizeof(ValBuf));
    if (!val_gptr.IsValid())
        return -1;

    //std::cout << " allocated memory at " << val_gptr << std::endl;

    ValBuf *val_p = (ValBuf*)mmgr_->GlobalToLocal(val_gptr);

    val_p->size = val_len;
    memcpy((char*)val_p->val, (char const *)val, val_len);
    fam_persist(val_p, sizeof(ValBuf)+val_len);

    TagGptr old_value;
    val_ptr = tree_->putC(key_ptr, val_gptr, old_value);
    if (old_value.IsValid()) {
        heap_->Free(op, old_value.gptr());
    }

    return 0;
}

int KVSRadixTree::Get (char const *key, size_t const key_len,
                       char *val, size_t &val_len,
                       Gptr &key_ptr, TagGptr &val_ptr) {
    //std::cout << "GET" << " " << std::string(key, key_len) << std::endl;

    if (key_len > kMaxKeyLen)
        return -1;

    Eop op(emgr_);

    std::pair<Gptr, TagGptr> kv_ptr = tree_->getC(key, key_len);

    key_ptr=kv_ptr.first; // key_ptr could be null
    val_ptr=kv_ptr.second; // val_ptr could be null

    if(!kv_ptr.first.IsValid()) {
        // key node does not exist
        //return -2;
        return 0;
    }
    // key node exists
    if (kv_ptr.second.IsValid()) {
        // val ptr is not null
        ValBuf *val_p = (ValBuf*)mmgr_->GlobalToLocal(kv_ptr.second.gptr());
        fam_invalidate(&val_p->size, sizeof(size_t));
        size_t val_size = val_p->size;
        if(val_len < val_size) {
            std::cout << "  val buffer is too small: " << val_len << " -> " << val_size << std::endl;
            val_len = val_size;
            return -1;
        }
        val_len = val_p->size;
        fam_invalidate(&val_p->val, val_len);
        fam_memcpy((char*)val, (char*)val_p->val, val_len);
    }

    return 0;
}

int KVSRadixTree::Get (Gptr const key_ptr, TagGptr &val_ptr,
                       char *val, size_t &val_len, bool get_value) {
    //std::cout << "GET" << " " << std::string(key, key_len) << std::endl;

    if (val_len > kMaxValLen)
        return -1;

    Eop op(emgr_);

    TagGptr val_ptr_cur = tree_->getC(key_ptr);

    if(val_ptr_cur == val_ptr && get_value==false) {
        // val_ptr is not stale
        return 0;
    }
    else {
        // val_ptr is stale or we always want to get the value
        if (val_ptr_cur.IsValid()) {
            // cur val ptr is not null
            ValBuf *val_p = (ValBuf*)mmgr_->GlobalToLocal(val_ptr_cur.gptr());
            fam_invalidate(&val_p->size, sizeof(size_t));
            size_t val_size = val_p->size;
            if(val_len < val_size) {
                std::cout << "  val buffer is too small: " << val_len << " -> " << val_size << std::endl;
                val_len = val_size;
                return -1;
            }
            val_len = val_p->size;
            fam_invalidate(&val_p->val, val_len);
            fam_memcpy((char*)val, (char*)val_p->val, val_len);
        }
        val_ptr=val_ptr_cur;
        return 0;
    }
}


int KVSRadixTree::Del (char const *key, size_t const key_len,
                       Gptr &key_ptr, TagGptr &val_ptr){
    //std::cout << "DEL" << " " << std::string(key, key_len) << std::endl;
    if (key_len > kMaxKeyLen)
        return -1;

    Eop op(emgr_);

    TagGptr old_value;
    std::pair<Gptr, TagGptr> kv_ptr = tree_->destroyC(key, key_len, old_value);

    key_ptr=kv_ptr.first; // key_ptr could be null
    val_ptr=kv_ptr.second; // val_ptr must be null

    if(!kv_ptr.first.IsValid()) {
        // key node does not exist
        //return -2;
        return 0;
    }
    // key node exists
    if (old_value.IsValid()) {
        // old_value is not null
        heap_->Free(op, old_value.gptr());
    }

    return 0;
}


int KVSRadixTree::Del (Gptr const key_ptr, TagGptr &val_ptr) {
    //std::cout << "DEL" << " " << std::string(key, key_len) << std::endl;
    Eop op(emgr_);

    TagGptr old_value;
    val_ptr = tree_->destroyC(key_ptr, old_value);
    if (old_value.IsValid()) {
        heap_->Free(op, old_value.gptr());
    }
    return 0;
}

void KVSRadixTree::ReportMetrics() {
    if (metrics_) {
        metrics_->Report();
    }
}

/* This function is used when one wants to get an element if exists
 * else insert the same to the tree in an atomic manner.
 *
 * Return value
 *         -1  Failed to find or create.
 *          0  item was found. 
 *          1  Item was inserted.
 * These values help the caller to take necessary actions.
 */          
int KVSRadixTree::FindOrCreate(char const *key, size_t const key_len,
                       char const *val, size_t const val_len, char *ret_val, size_t &ret_len) {

    if (key_len > kMaxKeyLen)
        return -1;
    if (val_len > kMaxValLen)
        return -1;

    Eop op(emgr_);

    Gptr val_gptr = heap_->Alloc(op, val_len+sizeof(ValBuf));
    if (!val_gptr.IsValid())
        return -1;

    ValBuf *val_ptr = (ValBuf*)mmgr_->GlobalToLocal(val_gptr);

    val_ptr->size = val_len;
    memcpy((char*)val_ptr->val, (char const *)val, val_len);
    fam_persist(val_ptr, sizeof(ValBuf)+val_len);

    TagGptr old_value = tree_->put(key, key_len, val_gptr, FIND_OR_CREATE);
    if (old_value.IsValid()) {

        LOG(trace) <<"KVSRadixTree::FindOrCreate(): Returning the found Entry\n" << std::endl;
        heap_->Free(op, val_gptr);

        ValBuf *val_p = (ValBuf*)mmgr_->GlobalToLocal(old_value.gptr());
        fam_invalidate(&val_p->size, sizeof(size_t));
        size_t val_size = val_p->size;
        if(ret_len < val_size) {
            LOG(trace) << "  val buffer is too small: " << ret_len << " -> " << val_size << std::endl;
            ret_len = val_size;
            return -1;
        }
        ret_len = val_size;
        fam_invalidate(&val_p->val, ret_len);
        assert(ret_val != nullptr);
        fam_memcpy((char*)ret_val, (char*)val_p->val, ret_len);

        return 0;
    } else {
         LOG(trace) << "  successfully inserted "
                   << std::string(key, key_len) << " = " << std::string(val, val_len) << std::endl;

        return 1;
    }

}

} // namespace radixtree
