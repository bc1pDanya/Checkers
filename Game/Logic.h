#pragma once
#include <random>
#include <vector>
#include <algorithm>
#include <ctime>
#include <string>
#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine (
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    vector<move_pos> find_best_turns(const bool color)
{
    next_best_state.clear();
    next_move.clear();

    // Запускаем поиск (начальное состояние — текущая доска).
    find_first_best_turn(board->get_board(), color, -1, -1, 0);

    // Восстанавливаем найденную цепочку ходов по индексам.
    int cur_state = 0;
    vector<move_pos> res;
    do
    {
        res.push_back(next_move[cur_state]);
        cur_state = next_best_state[cur_state];
    } while (cur_state != -1 && next_move[cur_state].x != -1);
    return res;
}


private:

// Ищет лучший ход (и возможную цепочку взятий) начиная с конкретной клетки/цепочки.
// mtx - текущая доска, color - текущий игрок, (x,y) - если != -1, то ищем ходы для этой фигуры,
// state - индекс состояния в next_move/next_best_state, alpha - параметр для отсечения.
double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
    double alpha = -1)
{
    // Регистрируем новое состояние
    next_best_state.push_back(-1);
    next_move.emplace_back(-1, -1, -1, -1);

    double best_score = -1;

    if (state != 0)
        find_turns(x, y, mtx);

    auto turns_now = turns;
    bool have_beats_now = have_beats;

    // Если цепочка ударов для этой фигуры закончилась (не было бьющих ходов) и это не начальное состояние —
    // переключаем сторону и используем общий рекурсивный поиск.
    if (!have_beats_now && state != 0)
    {
        return find_best_turns_rec(mtx, 1 - color, 0, alpha);
    }

    for (auto turn : turns_now)
    {
        size_t next_state = next_move.size();
        double score;

        if (have_beats_now)
        {
            // продолжаем цепочку (цвет не переключается)
            score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, next_state, best_score);
        }
        else
        {
            // обычный ход — переключаем цвет
            score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
        }

        if (score > best_score)
        {
            best_score = score;
            next_best_state[state] = (have_beats_now ? int(next_state) : -1);
            next_move[state] = turn;
        }
    }

    return best_score;
}

// Рекурсивный minimax-поиск с (опциональным) alpha-beta отсечением.
// mtx - позиция, color - текущий игрок, depth - глубина,
// alpha/beta - параметры отсечения, x/y - если заданы, ищем ходы для конкретной фигуры (цепочка взятий).
double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
    double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)
{
    // Базовый случай
    if (depth == Max_depth)
    {
        return calc_score(mtx, (depth % 2 == color));
    }

    if (x != -1)
    {
        find_turns(x, y, mtx);
    }
    else
    {
        find_turns(color, mtx);
    }

    auto turns_now = turns;
    bool have_beats_now = have_beats;

    // Если в цепочке удары закончились — переключаем игрока и глубину
    if (!have_beats_now && x != -1)
    {
        return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
    }

    // Терминальное состояние: ходов нет
    if (turns.empty())
        return (depth % 2 ? 0 : INF);

    double min_score = INF + 1;
    double max_score = -1;

    for (auto turn : turns_now)
    {
        double score = 0.0;
        if (!have_beats_now && x == -1)
        {
            score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
        }
        else
        {
            score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
        }

        min_score = std::min(min_score, score);
        max_score = std::max(max_score, score);

        if (depth % 2)
            alpha = std::max(alpha, max_score);
        else
            beta = std::min(beta, min_score);

        if (optimization != "O0" && alpha >= beta)
            return (depth % 2 ? max_score + 1 : min_score - 1);
    }

    return (depth % 2 ? max_score : min_score);
}

    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }

    // Вычисляет "оценку" позиции mtx для алгоритма принятия решений бота.
    // first_bot_color — цвет, за который играет бот (true/false).
    // Логика:
    //  1) Подсчёт количества белых/чёрных шашек и дамок.
    //  2) При режиме "NumberAndPotential" учитывается потенциальное продвижение вперёд
    //     (добавляются небольшие бонусы за близость к превращению в дамку).
    //  3) Если у одной из сторон нет фигур — возвращается +∞ (поражение) или 0 (победа).
    //  4) Для дамок применяется повышающий коэффициент q_coef (по умолчанию 4 или 5).
    //  5) Итоговая оценка = (фигуры соперника) / (фигуры бота) с учётом коэффициентов.
    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
    {
        // Счётчики для шашек и дамок обеих сторон
        double w = 0, wq = 0, b = 0, bq = 0;

        // Подсчёт фигур
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w  += (mtx[i][j] == 1);
                wq += (mtx[i][j] == 3);
                b  += (mtx[i][j] == 2);
                bq += (mtx[i][j] == 4);

                // Если включен режим "NumberAndPotential" — учитываем продвижение шашек вперёд
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i); // Белые — чем ближе к дамке, тем больше вес
                    b += 0.05 * (mtx[i][j] == 2) * (i);     // Чёрные — аналогично
                }
            }
        }

        // Если бот играет за чёрных — меняем местами счётчики
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }

        // Если у бота нет фигур — поражение
        if (w + wq == 0)
            return INF;

        // Если у соперника нет фигур — победа
        if (b + bq == 0)
            return 0;

        // Коэффициент ценности дамки
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }

        // Чем меньше результат, тем лучше для бота (мы считаем "опасность" позиции)
        return (b + bq * q_coef) / (w + wq * q_coef);
    }


    
public:
    // Вызов find_turns по цвету игрока.
    // Использует текущее состояние доски из объекта board.
    // Находит все возможные ходы для фигур заданного цвета.
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    // Вызов find_turns по координатам фигуры.
    // Использует текущее состояние доски из объекта board.
    // Находит все возможные ходы для фигуры, стоящей в (x, y).
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // Основной метод поиска ходов по цвету игрока в переданной матрице доски mtx.
    // Обходит все клетки, находит фигуры указанного цвета и собирает их возможные ходы.
    // Если есть хотя бы один рубящий ход, сохраняет только рубящие ходы (по правилам шашек).
    // Перемешивает список ходов для случайности (если рандом включен).
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    // Проверка возможных ходов для каждой фигуры
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        // Если впервые встретили бьющий ход — очищаем предыдущие
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        // Добавляем ходы в общий список
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    // Метод поиска ходов для одной фигуры по её координатам в переданной матрице доски mtx.
    // 1) Проверяет рубящие ходы (для шашек и дамок).
    // 2) Если рубящих ходов нет — проверяет обычные ходы.
    // Устанавливает флаг have_beats = true, если найдены рубящие ходы.
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];

        // --- Проверка рубящих ходов ---
        switch (type)
        {
        case 1:
        case 2:
            // Проверка шашек (обычных фигур)
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // Проверка дамок (ход по диагоналям на любое расстояние)
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j;
                         i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1;
                         i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        // Если есть хотя бы один рубящий ход — выходим
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }

        // --- Проверка обычных ходов ---
        switch (type)
        {
        case 1:
        case 2:
            // Для шашек (только вперёд на одну клетку по диагонали)
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // Для дамок (любой свободный ход по диагонали)
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j;
                         i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1;
                         i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public:
    // Список возможных ходов, найденных последним вызовом find_turns().
    // Каждый элемент — структура move_pos с координатами хода.
    vector<move_pos> turns;

    // Флаг: true, если среди найденных ходов есть хотя бы один рубящий (взятие фигуры).
    bool have_beats;

    // Максимальная глубина поиска в рекурсивных алгоритмах (используется ботом для выбора хода).
    int Max_depth;

  private:
   // Генератор случайных чисел для перемешивания ходов (если включён рандом).
    default_random_engine rand_eng;

    // Режим подсчёта оценки позиции:
    // "Number" — учитывается только количество фигур,
    // "NumberAndPotential" — также учитывается продвижение к превращению в дамку.
    string scoring_mode;

    // Режим оптимизации поиска:
    // "O0" — без оптимизации,
    // другие значения — включают alpha-beta pruning и прочие оптимизации.
    string optimization;

    // next_move[state] хранит ход, который был выбран из позиции с индексом state.
    // Используется для восстановления цепочки ходов в find_best_turns().
    vector<move_pos> next_move;

    // next_best_state[state] хранит индекс следующего состояния (позиций) для выбранного хода.
    // -1, если это последний ход в цепочке.
    vector<int> next_best_state;

    // Указатель на объект Board — текущая доска.
    Board *board;

    // Указатель на объект Config — конфигурация бота и настроек игры.
    Config *config;

};
