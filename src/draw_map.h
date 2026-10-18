#pragma once
#include "basetype.h"

class COLOR;
class INPUT;

class DRAW_MAP
{
public:
    DRAW_MAP(float x, float y, float width, float height);
    void Draw();
    int Control(INPUT* input);
    int IsClickOnMap(const INPUT* input) const;
    void ClickOnMap(const INPUT* input) const;

    VECTOR2 ToScreen(const VECTOR2& point) const;
    VECTOR2 FromScreen(const VECTOR2& point) const;
    int IsInside(const VECTOR2& point) const;
    void DrawDot(VECTOR2 point, COLOR color) const;
    void DrawDot2x2(VECTOR2 point, COLOR color) const;
    void DrawDot4x4(VECTOR2 point, COLOR color) const;

private:
    unsigned int optionBits;
    VECTOR2 origin;
    VECTOR2 size;
};
