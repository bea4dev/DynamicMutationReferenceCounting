#pragma once

#include <cstddef>
#include <atomic>
#include <optional>

using namespace std;


#if RC_VALIDATION
    atomic_size_t global_object_count(0);
#endif


/**
 * オブジェクトのヘッダ部分
 */
class HeapObject{

public:
    //参照カウント
    size_t reference_count;
    //フィールドの長さ
    size_t field_length;
    //このオブジェクトが複数のスレッドからアクセスされる可能性があるかどうか
    //詳細は"dynamic_rc_hpp"を参照
    bool is_mutex;
    //スピンロックに使用するためのフラグ
    atomic_flag spin_lock_flag;


    /**
     * このオブジェクト以下のオブジェクト(フィールドに間接的に連なる全てのオブジェクトを含む)の is_mutex を true に伝搬させる
     * 詳細は"dynamic_rc_hpp"を参照
     */
    inline void to_mutex() {
        //is_mutex が false である場合
        if (!this->is_mutex) {
            this->is_mutex = true;

            auto field_length = this->field_length;
            //フィールドの開始ポインタ
            auto** field_start_ptr = (HeapObject**) (this + 1);

            for (size_t field_index = 0; field_index < field_length; field_index++) {
                //対象となるフィールドのポインタ
                auto** field_ptr = field_start_ptr + field_index;
                //フィールドの内容をロード
                auto* field_object = *field_ptr;

                if (field_object != nullptr) {
                    //再帰的に呼び出し
                    field_object->to_mutex();
                }
            }
        }
    }
};


/**
 * オブジェクトをヒープ領域に割り当て
 */
inline HeapObject* alloc_heap_object(size_t field_length) {
    //確保するサイズ
    //HeapObject をヘッダとしてそれに連なる形でフィールドの領域も合わせて確保
    auto allocate_size = sizeof(HeapObject) + sizeof(HeapObject*) * field_length;
    auto* object_ptr = (HeapObject*) malloc(allocate_size);
    
    //各フィールドを初期化
    //フィールドの開始ポインタ
    auto** field_start_ptr = (HeapObject**) (object_ptr + 1);
    for (size_t i = 0; i < field_length; i++) {
        *(field_start_ptr + i) = nullptr;
    }

    //ヘッダの各フィールドを初期化
    object_ptr->is_mutex = false;
    object_ptr->reference_count = 1;
    object_ptr->field_length = field_length;
    *((bool*) &object_ptr->spin_lock_flag) = false;

    #if RC_VALIDATION
        //生存しているオブジェクト数を一つ増やす
        global_object_count.fetch_add(1, memory_order_relaxed);
    #endif

    return object_ptr;
}