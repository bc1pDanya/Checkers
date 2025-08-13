#pragma once  // Гарантирует однократное включение файла при компиляции
#include <iostream>
#include <fstream>
#include <vector>

// Включение пользовательских заголовочных файлов
#include "../Models/Move.h"       // Определение структуры хода (move_pos)
#include "../Models/Project_path.h" // Пути к ресурсам проекта

// Обработка разных путей включения для SDL в зависимости от ОС
#ifdef __APPLE__
    #include <SDL.h>
    #include <SDL_image.h>
#else
    #include <SDL.h>
    #include <SDL_image.h>
#endif

using namespace std;  // Использование стандартного пространства имен

// Класс, представляющий игровую доску и её визуализацию
class Board
{
public:
    Board() = default;  // Конструктор по умолчанию
    
    // Конструктор с установкой размеров окна
    Board(const unsigned int W, const unsigned int H) : W(W), H(H)
    {
    }

    // Инициализация и отрисовка стартового состояния доски
    int start_draw()
    {
        // Инициализация SDL2
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        {
            print_exception("SDL_Init can't init SDL2 lib");
            return 1;
        }
        
        // Автоматический расчет размеров окна если не заданы явно
        if (W == 0 || H == 0)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm))
            {
                print_exception("SDL_GetDesktopDisplayMode can't get desctop display mode");
                return 1;
            }
            // Установка размеров (квадратная доска)
            W = min(dm.w, dm.h);
            W -= W / 15;  // Отступ от края экрана
            H = W;
        }
        
        // Создание окна с заголовком "Checkers"
        win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE);
        if (win == nullptr)
        {
            print_exception("SDL_CreateWindow can't create window");
            return 1;
        }
        
        // Создание аппаратно-ускоренного рендерера с вертикальной синхронизацией
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ren == nullptr)
        {
            print_exception("SDL_CreateRenderer can't create renderer");
            return 1;
        }
        
        // Загрузка текстур для всех элементов игры
        board = IMG_LoadTexture(ren, board_path.c_str());        // Текстура доски
        w_piece = IMG_LoadTexture(ren, piece_white_path.c_str()); // Белая шашка
        b_piece = IMG_LoadTexture(ren, piece_black_path.c_str()); // Черная шашка
        w_queen = IMG_LoadTexture(ren, queen_white_path.c_str()); // Белая дамка
        b_queen = IMG_LoadTexture(ren, queen_black_path.c_str()); // Черная дамка
        back = IMG_LoadTexture(ren, back_path.c_str());          // Кнопка "Назад"
        replay = IMG_LoadTexture(ren, replay_path.c_str());      // Кнопка "Повтор игры"
        
        // Проверка успешной загрузки всех текстур
        if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back || !replay)
        {
            print_exception("IMG_LoadTexture can't load main textures from " + textures_path);
            return 1;
        }
        
        // Обновление реальных размеров окна после создания
        SDL_GetRendererOutputSize(ren, &W, &H);
        make_start_mtx();  // Создание начальной расстановки шашек
        rerender();        // Первоначальная отрисовка
        return 0;          // Успешное завершение
    }

    // Сброс доски в начальное состояние
    void redraw()
    {
        game_results = -1;  // Сброс результатов игры
        history_mtx.clear();         // Очистка истории ходов
        history_beat_series.clear(); // Очистка истории серий взятий
        make_start_mtx();            // Создание стартовой позиции
        clear_active();               // Сброс активной клетки
        clear_highlight();           // Очистка подсветки
    }

    // Перемещение шашки по данным из структуры Move
    void move_piece(move_pos turn, const int beat_series = 0)
    {
        // Если указана позиция шашки для взятия (xb, yb)
        if (turn.xb != -1)
        {
            // Удаление взятой шашки с доски
            mtx[turn.xb][turn.yb] = 0;
        }
        // Вызов основного метода перемещения
        move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series);
    }

    // Основной метод перемещения шашки
    void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2, const int beat_series = 0)
    {
        // Проверка: целевая позиция должна быть пустой
        if (mtx[i2][j2])
        {
            throw runtime_error("final position is not empty, can't move");
        }
        // Проверка: исходная позиция должна содержать шашку
        if (!mtx[i][j])
        {
            throw runtime_error("begin position is empty, can't move");
        }
        
        // Превращение в дамку (белая шашка на 0-й линии или черная на 7-й)
        if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7))
            mtx[i][j] += 2;  // 1->3 (белая дамка), 2->4 (черная дамка)
            
        // Перемещение фигуры в новую позицию
        mtx[i2][j2] = mtx[i][j];
        drop_piece(i, j);  // Очистка исходной позиции
        
        // Сохранение состояния в истории (с указанием серии взятий)
        add_history(beat_series);
    }

    // Удаление шашки с доски
    void drop_piece(const POS_T i, const POS_T j)
    {
        mtx[i][j] = 0;   // Очистка клетки
        rerender();       // Перерисовка доски
    }

    // Превращение шашки в дамку
    void turn_into_queen(const POS_T i, const POS_T j)
    {
        // Проверка: только обычные шашки могут превращаться в дамку
        if (mtx[i][j] == 0 || mtx[i][j] > 2)
        {
            throw runtime_error("can't turn into queen in this position");
        }
        mtx[i][j] += 2;  // 1->3, 2->4
        rerender();      // Перерисовка
    }
    
    // Получение текущего состояния доски
    vector<vector<POS_T>> get_board() const
    {
        return mtx;
    }

    // Подсветка указанных клеток (для показа возможных ходов)
    void highlight_cells(vector<pair<POS_T, POS_T>> cells)
    {
        for (auto pos : cells)
        {
            POS_T x = pos.first, y = pos.second;
            is_highlighted_[x][y] = 1;  // Активация подсветки
        }
        rerender();  // Перерисовка с обновленной подсветкой
    }

    // Сброс всей подсветки
    void clear_highlight()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            is_highlighted_[i].assign(8, 0);  // Отключение подсветки для всех клеток
        }
        rerender();
    }

    // Установка активной клетки (текущая выбранная шашка)
    void set_active(const POS_T x, const POS_T y)
    {
        active_x = x;
        active_y = y;
        rerender();  // Перерисовка с выделением активной клетки
    }

    // Сброс активной клетки
    void clear_active()
    {
        active_x = -1;  // Инвалидация координат
        active_y = -1;
        rerender();
    }

    // Проверка подсвечена ли указанная клетка
    bool is_highlighted(const POS_T x, const POS_T y)
    {
        return is_highlighted_[x][y];
    }

    // Отмена последнего хода (возврат к предыдущему состоянию)
    void rollback()
    {
        // Определение сколько шагов нужно откатить (серия взятий)
        auto beat_series = max(1, *(history_beat_series.rbegin()));
        
        // Откат всей серии ходов
        while (beat_series-- && history_mtx.size() > 1)
        {
            history_mtx.pop_back();          // Удаление состояний
            history_beat_series.pop_back();  // Удаление записей о взятиях
        }
        
        // Восстановление предыдущего состояния доски
        mtx = *(history_mtx.rbegin());
        
        // Сброс UI-состояний
        clear_highlight();
        clear_active();
    }

    // Отображение итогов игры (окно с результатом)
    void show_final(const int res)
    {
        game_results = res;  // Сохранение результата (1-белые, 2-черные, др.-ничья)
        rerender();          // Перерисовка с отображением результата
    }

    // Обновление размеров окна (при изменении пользователем)
    void reset_window_size()
    {
        SDL_GetRendererOutputSize(ren, &W, &H);  // Получение актуальных размеров
        rerender();                               // Перерисовка с новыми размерами
    }

    // Корректное освобождение ресурсов SDL
    void quit()
    {
        // Уничтожение текстур
        SDL_DestroyTexture(board);
        SDL_DestroyTexture(w_piece);
        SDL_DestroyTexture(b_piece);
        SDL_DestroyTexture(w_queen);
        SDL_DestroyTexture(b_queen);
        SDL_DestroyTexture(back);
        SDL_DestroyTexture(replay);
        
        // Уничтожение рендерера и окна
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        
        // Выгрузка SDL
        SDL_Quit();
    }

    // Деструктор - автоматическая очистка ресурсов
    ~Board()
    {
        if (win)
            quit();
    }

private:
    // Добавление текущего состояния доски в историю
    void add_history(const int beat_series = 0)
    {
        history_mtx.push_back(mtx);           // Сохранение состояния
        history_beat_series.push_back(beat_series);  // Сохранение информации о взятиях
    }
    
    // Создание начальной расстановки шашек
    void make_start_mtx()
    {
        // Инициализация матрицы 8x8 нулями
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                mtx[i][j] = 0;  // Пустая клетка
                
                // Расстановка черных шашек (верхние 3 ряда)
                if (i < 3 && (i + j) % 2 == 1)
                    mtx[i][j] = 2;
                
                // Расстановка белых шашек (нижние 3 ряда)
                if (i > 4 && (i + j) % 2 == 1)
                    mtx[i][j] = 1;
            }
        }
        add_history();  // Сохранение начальной позиции в истории
    }

    // Основной метод перерисовки интерфейса
    void rerender()
    {
        // Очистка рендерера
        SDL_RenderClear(ren);
        
        // Отрисовка фона (доска)
        SDL_RenderCopy(ren, board, NULL, NULL);

        // Отрисовка всех фигур на доске
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!mtx[i][j])  // Пропуск пустых клеток
                    continue;
                    
                // Расчет позиции для отрисовки (центрирование в клетке)
                int wpos = W * (j + 1) / 10 + W / 120;
                int hpos = H * (i + 1) / 10 + H / 120;
                SDL_Rect rect{ wpos, hpos, W / 12, H / 12 };

                // Выбор текстуры в зависимости от типа фигуры
                SDL_Texture* piece_texture;
                if (mtx[i][j] == 1)       piece_texture = w_piece;   // Белая шашка
                else if (mtx[i][j] == 2)   piece_texture = b_piece;   // Черная шашка
                else if (mtx[i][j] == 3)   piece_texture = w_queen;   // Белая дамка
                else                       piece_texture = b_queen;   // Черная дамка

                // Отрисовка фигуры
                SDL_RenderCopy(ren, piece_texture, NULL, &rect);
            }
        }

        // Настройка для отрисовки подсветки
        SDL_SetRenderDrawColor(ren, 0, 255, 0, 0);  // Зеленый цвет
        const double scale = 2.5;  // Масштаб для тонких линий
        SDL_RenderSetScale(ren, scale, scale);
        
        // Отрисовка подсвеченных клеток (зеленые рамки)
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!is_highlighted_[i][j])
                    continue;
                    
                // Расчет позиции и размеров рамки
                SDL_Rect cell{ 
                    int(W * (j + 1) / 10 / scale), 
                    int(H * (i + 1) / 10 / scale), 
                    int(W / 10 / scale),
                    int(H / 10 / scale) 
                };
                SDL_RenderDrawRect(ren, &cell);  // Отрисовка прямоугольника
            }
        }

        // Отрисовка активной клетки (красная рамка)
        if (active_x != -1)
        {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 0);  // Красный цвет
            SDL_Rect active_cell{ 
                int(W * (active_y + 1) / 10 / scale), 
                int(H * (active_x + 1) / 10 / scale),
                int(W / 10 / scale), 
                int(H / 10 / scale) 
            };
            SDL_RenderDrawRect(ren, &active_cell);
        }
        SDL_RenderSetScale(ren, 1, 1);  // Сброс масштаба

        // Отрисовка кнопок управления
        // Кнопка "Назад" в левом верхнем углу
        SDL_Rect rect_left{ W / 40, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, back, NULL, &rect_left);
        
        // Кнопка "Повтор игры" в правом верхнем углу
        SDL_Rect replay_rect{ W * 109 / 120, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, replay, NULL, &replay_rect);

        // Отображение результатов игры (если есть)
        if (game_results != -1)
        {
            string result_path = draw_path;  // По умолчанию ничья
            
            // Выбор текстуры по результату
            if (game_results == 1)       result_path = white_path;  // Белые победили
            else if (game_results == 2)  result_path = black_path;  // Черные победили
            
            // Загрузка текстуры результата
            SDL_Texture* result_texture = IMG_LoadTexture(ren, result_path.c_str());
            if (result_texture == nullptr)
            {
                print_exception("IMG_LoadTexture can't load game result picture from " + result_path);
                return;
            }
            
            // Позиционирование по центру экрана
            SDL_Rect res_rect{ W / 5, H * 3 / 10, W * 3 / 5, H * 2 / 5 };
            SDL_RenderCopy(ren, result_texture, NULL, &res_rect);
            
            // Очистка временной текстуры
            SDL_DestroyTexture(result_texture);
        }

        // Обновление экрана
        SDL_RenderPresent(ren);
        
        // Короткая задержка и обработка событий (особенно важно для macOS)
        SDL_Delay(10);
        SDL_Event windowEvent;
        SDL_PollEvent(&windowEvent);
    }

    // Логирование ошибок в файл
    void print_exception(const string& text) {
        ofstream fout(project_path + "log.txt", ios_base::app);  // Открытие в режиме дополнения
        fout << "Error: " << text << ". "<< SDL_GetError() << endl;  // Запись ошибки и кода SDL
        fout.close();
    }

public:
    int W = 0;  // Ширина окна
    int H = 0;  // Высота окна
    
    // История состояний доски (для реализации отмены хода)
    vector<vector<vector<POS_T>>> history_mtx;

private:
    // Указатели на ресурсы SDL
    SDL_Window *win = nullptr;     // Окно приложения
    SDL_Renderer *ren = nullptr;   // Рендерер для отрисовки
    
    // Текстуры элементов игры
    SDL_Texture *board = nullptr;   // Игровая доска
    SDL_Texture *w_piece = nullptr; // Белая шашка
    SDL_Texture *b_piece = nullptr; // Черная шашка
    SDL_Texture *w_queen = nullptr; // Белая дамка
    SDL_Texture *b_queen = nullptr; // Черная дамка
    SDL_Texture *back = nullptr;    // Кнопка "Назад"
    SDL_Texture *replay = nullptr;  // Кнопка "Повтор игры"
    
    // Пути к файлам текстур
    const string textures_path = project_path + "Textures/";  // Базовый путь
    const string board_path = textures_path + "board.png";
    const string piece_white_path = textures_path + "piece_white.png";
    const string piece_black_path = textures_path + "piece_black.png";
    const string queen_white_path = textures_path + "queen_white.png";
    const string queen_black_path = textures_path + "queen_black.png";
    const string white_path = textures_path + "white_wins.png";   // Белые победили
    const string black_path = textures_path + "black_wins.png";   // Черные победили
    const string draw_path = textures_path + "draw.png";          // Ничья
    const string back_path = textures_path + "back.png";          // Стрелка назад
    const string replay_path = textures_path + "replay.png";      // Иконка повтора
    
    // Состояние интерфейса
    int active_x = -1, active_y = -1;  // Координаты активной (выбранной) клетки
    int game_results = -1;             // Результат игры (-1 - игра продолжается)
    
    // Матрица подсветки (8x8) - показывает возможные ходы
    vector<vector<bool>> is_highlighted_ = vector<vector<bool>>(8, vector<bool>(8, 0));
    
    // Матрица состояния доски (8x8):
    // 0 - пусто, 1 - белая шашка, 2 - черная шашка, 
    // 3 - белая дамка, 4 - черная дамка
    vector<vector<POS_T>> mtx = vector<vector<POS_T>>(8, vector<POS_T>(8, 0));
    
    // История серий взятий (для корректного отката ходов)
    vector<int> history_beat_series;
};