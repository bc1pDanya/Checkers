#pragma once
#include <stdlib.h>

// POS_T — тип для хранения координат клетки (int8_t = целое от -128 до 127).
typedef int8_t POS_T;

// Структура move_pos описывает один ход на доске
struct move_pos
{
    POS_T x, y;             // Начальная позиция фигуры (откуда ходим)
    POS_T x2, y2;           // Конечная позиция фигуры (куда ходим)
    POS_T xb = -1, yb = -1; // Координаты сбитой фигуры (если ход с взятием). 
                            // -1 означает, что фигура не бита.

    // Конструктор для обычного хода без взятия
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2) 
        : x(x), y(y), x2(x2), y2(y2)
    {
    }

    // Конструктор для хода со взятием (включает координаты сбитой фигуры)
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2, const POS_T xb, const POS_T yb)
        : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb)
    {
    }

    // Сравнение двух ходов: равны, если совпадают начальная и конечная позиции
    bool operator==(const move_pos &other) const
    {
        return (x == other.x && y == other.y && x2 == other.x2 && y2 == other.y2);
    }

    // Проверка на неравенство (просто отрицание оператора ==)
    bool operator!=(const move_pos &other) const
    {
        return !(*this == other);
    }
};
