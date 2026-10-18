#include "sprite.h"
#include "myerror.h"
#include <new>

template<> ACT* LIST<ACT>::operator[](int index) { return m_data+index; }
template<> const ACT* LIST<ACT>::operator[](int index) const { return m_data+index; }
template<> int LIST<ACT>::No() const { return m_count; }

template<> void LIST<ACT>::Release()
{
    m_capacity=0;
    m_count=0;
    if (m_data) { delete[] m_data; m_data=0; }
}

template<> LIST<ACT>& LIST<ACT>::operator=(const LIST<ACT>& source)
{
    if (this==&source) return *this;
    Release();
    m_count=source.m_count;
    m_capacity=source.m_capacity;
    m_data=new(std::nothrow) ACT[m_capacity];
    if (!m_data) ::Error->LogExit(STRING("!!!ERROR!!!::LIST: Not enough memory for = %i"),m_capacity);
    for (int i=0;i<m_count;++i) m_data[i]=source.m_data[i];
    return *this;
}

template<> int LIST<ACT>::DeleteNumberS(int index)
{
    if (index<0 || index>=m_count) return 1;
    --m_count;
    for (int i=index;i<m_count;++i) m_data[i]=m_data[i+1];
    if (!m_count) Release();
    return 0;
}

template<> void LIST<ACT>::ShiftUp(int index)
{
    if (!index) return;
    const ACT previous=m_data[index-1];
    m_data[index-1]=m_data[index];
    m_data[index]=previous;
}

template<> void LIST<ACT>::ShiftDown(int index)
{
    if (index>=m_count-1) return;
    const ACT next=m_data[index+1];
    m_data[index+1]=m_data[index];
    m_data[index]=next;
}
