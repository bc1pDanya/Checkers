#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс Hand отвечает за обработку ввода игрока (мышь, закрытие окна и т.п.)
class Hand
{
  public:
    // Конструктор принимает указатель на объект Board, чтобы понимать размеры и состояние доски
    Hand(Board *board) : board(board)
    {
    }

    // Ожидает клик игрока по клетке или другое событие.
    // Возвращает кортеж: (тип ответа, координата X, координата Y).
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;   // Переменная для событий SDL
        Response resp = Response::OK; // По умолчанию — никаких особых действий
        int x = -1, y = -1;      // Пиксельные координаты клика
        int xc = -1, yc = -1;    // Логические координаты клетки на доске

        while (true)
        {
            // Проверяем, есть ли события в очереди
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    // Пользователь закрыл окно — передаём сигнал завершения игры
                    resp = Response::QUIT;
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    // Получаем пиксельные координаты клика
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;

                    // Преобразуем пиксельные координаты в координаты клеток (xc, yc)
                    // Высота/ширина клетки вычисляется как (высота окна / 10)
                    // "-1" компенсирует рамки/панель
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);

                    // Обработка специальных зон интерфейса:
                    // Левая верхняя ячейка (-1,-1) = кнопка "назад", доступна если есть история ходов
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK;
                    }
                    // Левая верхняя строка и 9-й столбец (-1,8) = кнопка "переиграть"
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY;
                    }
                    // Клик по рабочей области доски (8x8)
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL;
                    }
                    // Клик вне допустимых областей — сброс координат
                    else
                    {
                        xc = -1;
                        yc = -1;
                    }
                    break;

                case SDL_WINDOWEVENT:
                    // Изменение размера окна — пересчитываем размеры элементов
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size();
                        break;
                    }
                }

                // Если получили какой-то ответ (не OK) — выходим из цикла
                if (resp != Response::OK)
                    break;
            }
        }
        return {resp, xc, yc};
    }

    // Метод ожидания простого события (без выбора клетки) — например, нажатия кнопки "переиграть" или выхода
    Response wait() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;

        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    // Закрытие окна
                    resp = Response::QUIT;
                    break;

                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    // Перерасчёт размеров доски при изменении окна
                    board->reset_window_size();
                    break;

                case SDL_MOUSEBUTTONDOWN: {
                    // Определяем координаты клика в логические
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);

                    // (-1,8) — кнопка "переиграть"
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                break;
                }

                if (resp != Response::OK)
                    break;
            }
        }
        return resp;
    }

  private:
    Board *board; // Указатель на объект доски — нужен для расчёта координат и размеров
};
