#pragma once

#include "mylib.h"

template<class T>
class LIST
{
public:
    LIST();
    virtual ~LIST();
    LIST& operator=(const LIST& source);
    T* operator[](int index);
    const T* operator[](int index) const;
    T& Item(int index) const;
    void GetReleased(LIST* source);

    int No() const;
    void Push(const T& value);
    void Insert(T value);
    void InsertFirst(T value);
    void InsertBefore(int index,T value);
    int DeleteNumber(int index);
    int DeleteNumberS(int index);
    void ShiftUp(int index);
    void ShiftDown(int index);
    int Location(const T& value) const;
    void Release();
    void Expand(int requestedCapacity);
    void ExpandForInsert();
    T BeginIterateNon0(int* index) const;
    T NextIterateNon0(int* index) const;

    T* Data()
    {
        return m_data;
    }
    const T* Data() const
    {
        return m_data;
    }
    int Capacity() const
    {
        return m_capacity;
    }

public:

int m_count;
    int m_capacity;
    T*  m_data;
};

template<class T>
struct NAMED_LIST_STRUCT
{
    int m_hash;
    STRING m_name;
    T m_value;

    NAMED_LIST_STRUCT();
    NAMED_LIST_STRUCT& operator=(const NAMED_LIST_STRUCT& source);
    ~NAMED_LIST_STRUCT();
};

template<class T>
class NAMED_LIST : public LIST<NAMED_LIST_STRUCT<T> >
{
public:
    int Location(const STRING& name) const;
    void Insert(STRING name,T value);
    STRING Name(int index) const;
    static int GetHash(const char* name);

    T& operator[](int index)
    {
        return this->m_data[index].m_value;
    }

    T& Value(int index)
    {
        return this->m_data[index].m_value;
    }
    const T& Value(int index) const
    {
        return this->m_data[index].m_value;
    }
    NAMED_LIST_STRUCT<T>& Record(int index)
    {
        return this->m_data[index];
    }
    const NAMED_LIST_STRUCT<T>& Record(int index) const
    {
        return this->m_data[index];
    }
};

struct ACT;
template<> LIST<ACT>::LIST();
template<> LIST<ACT>::~LIST();
template<> LIST<ACT>& LIST<ACT>::operator=(const LIST<ACT>& source);
template<> ACT* LIST<ACT>::operator[](int index);
template<> const ACT* LIST<ACT>::operator[](int index) const;
template<> int LIST<ACT>::No() const;
template<> void LIST<ACT>::Insert(ACT value);
template<> void LIST<ACT>::InsertFirst(ACT value);
template<> void LIST<ACT>::InsertBefore(int index,ACT value);
template<> int LIST<ACT>::DeleteNumber(int index);
template<> int LIST<ACT>::DeleteNumberS(int index);
template<> void LIST<ACT>::ShiftUp(int index);
template<> void LIST<ACT>::ShiftDown(int index);
template<> void LIST<ACT>::Release();
template<> void LIST<ACT>::Expand(int requestedCapacity);

template<> LIST<int>::LIST();
template<> LIST<int>::~LIST();
template<> LIST<int>& LIST<int>::operator=(const LIST<int>& source);
template<> int* LIST<int>::operator[](int index);
template<> const int* LIST<int>::operator[](int index) const;
template<> int LIST<int>::No() const;
template<> void LIST<int>::Release();
template<> void LIST<int>::Insert(int value);
template<> void LIST<int>::ExpandForInsert();
template<> void LIST<int>::Expand(int requestedCapacity);

class SPRITE;

template<> LIST<SPRITE*>::LIST();
template<> LIST<SPRITE*>::~LIST();
template<> SPRITE** LIST<SPRITE*>::operator[](int index);
template<> SPRITE* const* LIST<SPRITE*>::operator[](int index) const;
template<> SPRITE*& LIST<SPRITE*>::Item(int index) const;
template<> int LIST<SPRITE*>::No() const;
template<> void LIST<SPRITE*>::Insert(SPRITE* value);
template<> void LIST<SPRITE*>::InsertFirst(SPRITE* value);
template<> void LIST<SPRITE*>::InsertBefore(int index, SPRITE* value);
template<> int LIST<SPRITE*>::DeleteNumber(int index);
template<> int LIST<SPRITE*>::Location(SPRITE* const& value) const;
template<> void LIST<SPRITE*>::Release();
template<> void LIST<SPRITE*>::Expand(int requestedCapacity);
template<> void LIST<SPRITE*>::ExpandForInsert();
template<> SPRITE* LIST<SPRITE*>::BeginIterateNon0(int* index) const;
template<> SPRITE* LIST<SPRITE*>::NextIterateNon0(int* index) const;
