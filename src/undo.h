#pragma once
#include "list.h"

class SPRITE;

class UNDO
{
public:
    struct UNDO_DATA
    {
        enum TYPE_CREATE { INSERTED = 0, DELETED = 1 };
        TYPE_CREATE type;
        SPRITE* sprite;

        UNDO_DATA();
        UNDO_DATA(TYPE_CREATE createType, SPRITE* object);
        void Undo();
        void DeleteSprite();
    };

    UNDO();
    ~UNDO();
    static void OnSpriteDestroyed(SPRITE* sprite);
    void Undo();
    void Redo();
    void Begin();
    void End();
    void AddInsert(SPRITE* sprite);
    void AddRemove(SPRITE* sprite);
    void DeleteLast();
    void Reset();
    int IsUndo() const;
    int IsRedo() const;

private:
    void ReleaseUndo(int index);
    void ForgetDestroyedSprite(SPRITE* sprite);
    enum { HistorySize = 5 };
    int noUndo;
    LIST<UNDO_DATA> sprites[HistorySize];
};

template<> LIST<UNDO::UNDO_DATA>::LIST();
template<> LIST<UNDO::UNDO_DATA>::~LIST();
template<> int LIST<UNDO::UNDO_DATA>::No() const;
template<> UNDO::UNDO_DATA* LIST<UNDO::UNDO_DATA>::operator[](int index);
template<> const UNDO::UNDO_DATA* LIST<UNDO::UNDO_DATA>::operator[](int index) const;
template<> void LIST<UNDO::UNDO_DATA>::Insert(UNDO::UNDO_DATA value);
template<> int LIST<UNDO::UNDO_DATA>::DeleteNumber(int index);
template<> void LIST<UNDO::UNDO_DATA>::Release();
template<> void LIST<UNDO::UNDO_DATA>::Expand(int capacity);
template<> void LIST<UNDO::UNDO_DATA>::ExpandForInsert();
template<> void LIST<UNDO::UNDO_DATA>::GetReleased(LIST<UNDO::UNDO_DATA>* source);
