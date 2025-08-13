#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
  public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // to start checkers
    int play()
    {
        // Записываем время старта партии для последующего логирования длительности игры.
        auto start = chrono::steady_clock::now();

        // Если включён режим "повтора" (replay) — перестраиваем логику и доску из сохранённого состояния,
        // перезагружаем конфиг (на случай изменения настроек) и перерисовываем доску.
        if (is_replay)
        {
            logic = Logic(&board, &config);
            config.reload();
            board.redraw();
        }
        else
        {
            // Стандартный запуск: отрисовать стартовое состояние игры/анимацию старта.
            board.start_draw();
        }
        is_replay = false;

        int turn_num = -1;           // номер текущего хода (увеличивается в начале каждого цикла)
        bool is_quit = false;        // флаг выхода по желанию игрока
        const int Max_turns = config("Game", "MaxNumTurns"); // ограничение по числу ходов

        // Главный цикл игры: выполняем до тех пор, пока не исчерпаем лимит ходов
        while (++turn_num < Max_turns)
        {
            beat_series = 0; // сбрасываем счётчик последовательных взятий для текущего хода

            // Генерируем все возможные ходы для текущего игрока (turn_num % 2 — цвет)
            logic.find_turns(turn_num % 2);

            // Если ходов нет — игра закончилась (например, блокировка / проигрыш)
            if (logic.turns.empty())
                break;

            // Устанавливаем глубину/уровень бота в зависимости от конфига и текущего цвета
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));

            // Если текущий цвет управляется человеком — запускаем обработку хода игрока
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                // Выполняем ход игрока: функция вернёт код ответа (QUIT / REPLAY / BACK / OK)
                auto resp = player_turn(turn_num % 2);

                // Обработка специальных ответов:
                if (resp == Response::QUIT)
                {
                    // Игрок выбрал выйти — запомним и выйдем из основного цикла
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY)
                {
                    // Игрок запросил перезапуск партии — включаем флаг и прерываем цикл
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK)
                {
                    // Откат хода: если включён бот для противоположной стороны и есть история — делаем rollback
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }
                    // Корректируем номер хода и откатываем историю на одну итерацию (повторный rollback)
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
                // Если ходит бот — вызываем функцию выполнения хода ботом
                bot_turn(turn_num % 2);
        }

        // Время окончания партии — логируем длительность игры в файл
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // Если был запрошен replay — просто запускаем play() заново (рекурсивно)
        if (is_replay)
            return play();

        // Если игрок выбрал quit — возвращаем 0 как код выхода
        if (is_quit)
            return 0;

        // Определяем результат партии:
        // res = 2 — неопределённый / предотвращённый вследствие выхода, 1/0 — победитель по цвету/ничья
        int res = 2;
        if (turn_num == Max_turns)
        {
            // Достигнут лимит ходов — считаем ничью
            res = 0;
        }
        else if (turn_num % 2)
        {
            // Если номер хода нечётный — побеждают белые (логика нумерации предполагает это)
            res = 1;
        }

        // Отображаем финальное состояние/экран окончания игры с указанием результата
        board.show_final(res);

        // Ждём ответа от интерфейса (возможный повтор, выход и т.п.)
        auto resp = hand.wait();
        if (resp == Response::REPLAY)
        {
            // Если пользователь захотел повтор — помечаем и перезапускаем игру
            is_replay = true;
            return play();
        }

        // Возвращаем окончательный результат игры
        return res;
    }

  private:
// Функция выполнения хода ботом
    void bot_turn(const bool color)  // color - цвет шашек бота (0-белые, 1-черные)
    {
        // Засекаем время начала хода для замера производительности
        auto start = chrono::steady_clock::now();

        // Получаем задержку для бота из конфига (имитация "думания")
        auto delay_ms = config("Bot", "BotDelayMS");
        
        // Запускаем поток с задержкой (чтобы бот не делал ходы мгновенно)
        thread th(SDL_Delay, delay_ms);
        
        // Параллельно ищем лучшие ходы для текущего цвета
        auto turns = logic.find_best_turns(color);
        
        // Дожидаемся завершения потока с задержкой
        th.join();
        
        bool is_first = true;  // Флаг первого хода в серии
        
        // Выполняем все ходы из найденной последовательности
        for (auto turn : turns)
        {
            // Для всех ходов кроме первого добавляем задержку (визуальное разделение)
            if (!is_first)
            {
                SDL_Delay(delay_ms);
            }
            is_first = false;
            
            // Увеличиваем счетчик серии взятий если был бой
            beat_series += (turn.xb != -1);
            
            // Применяем ход на доске
            board.move_piece(turn, beat_series);
        }

        // Фиксируем время окончания хода
        auto end = chrono::steady_clock::now();
        
        // Логируем время выполнения хода бота
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " 
            << (int)chrono::duration<double, milli>(end - start).count() 
            << " millisec\n";
        fout.close();
    }

    Response player_turn(const bool color)
    {
        // Собираем список стартовых клеток (откуда можно пойти) из сгенерированных логикой ходов
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            // turn.x, turn.y — исходные координаты хода
            cells.emplace_back(turn.x, turn.y);
        }

        // Подсветить на доске все клетки, с которых доступны ходы — визуальная подсказка игроку
        board.highlight_cells(cells);

        // pos — выбранный ход (from->to с информацией о бите), по умолчанию "пустой"
        move_pos pos = {-1, -1, -1, -1};
        // x,y — временно выбранная исходная клетка (при первой фазе выбора)
        POS_T x = -1, y = -1;

        // Фаза 1: выбор исходной клетки и целевой клетки (первый шаг хода)
        while (true)
        {
            // Ждём ввода от интерфейса (hand.get_cell возвращает tuple: (Response, x, y))
            auto resp = hand.get_cell();
            // Если пришёл не выбор клетки — передаём этот ответ вверх (QUIT / REPLAY / BACK и т.п.)
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp);

            // Получили координаты клетки от пользователя
            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

            // Проверяем, является ли выбранная клетка допустимой исходной клеткой
            bool is_correct = false;
            for (auto turn : logic.turns)
            {
                // Если пользователь выбрал одну из исходных клеток — запомним это
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }
                // Если пользователь уже выбрал исходную клетку (x,y) и сразу кликает на целевую,
                // проверяем, совпадает ли этот выбранный переход с каким-либо допустимым ходом.
                if (turn == move_pos{x, y, cell.first, cell.second})
                {
                    pos = turn; // нашли конкретный ход — запомним его и выйдем из выбора
                    break;
                }
            }

            // Если pos установлен — значит пользователь выбрал полный ход (from->to). Переходим дальше.
            if (pos.x != -1)
                break;

            // Если выбранная клетка не является корректной исходной...
            if (!is_correct)
            {
                // ...и при этом у нас уже была выделена какая-то активная исходная клетка,
                // то сбрасываем выделение и возвращаем стандартную подсветку всех возможных исходов.
                if (x != -1)
                {
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells);
                }
                // Сбрасываем временные координаты и продолжаем ждать ввода
                x = -1;
                y = -1;
                continue;
            }

            // Если дошли сюда — пользователь выбрал корректную исходную клетку (from).
            x = cell.first;
            y = cell.second;

            // Подчистим подсветку и пометим выбранную клетку как активную
            board.clear_highlight();
            board.set_active(x, y);

            // Соберём список возможных целей (куда можно пойти именно с этой клетки)
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            // Подсветим эти целевые клетки — помогая игроку увидеть варианты
            board.highlight_cells(cells2);
        }

        // Пользователь выбрал ход — очистим подсветки и применим ход
        board.clear_highlight();
        board.clear_active();
        board.move_piece(pos, pos.xb != -1);

        // Если это простой ход (без взятия) — возвращаем OK, ход завершён
        if (pos.xb == -1)
            return Response::OK;

        // Если ход был взятием — начинается серия взятий (multi-capture)
        beat_series = 1;

        // Цикл продолжения взятий: пока из текущей позиции есть доступные взятия, просим следующую клетку
        while (true)
        {
            // Сгенерировать возможные ходы (включая только продолжения взятий) из текущей целевой позиции
            logic.find_turns(pos.x2, pos.y2);
            // Если больше нет обязательных взятий — серия окончена
            if (!logic.have_beats)
                break;

            // Подсветить все возможные целевые клетки для продолжения взятия
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);
            // Сделать текущую клетку активной для удобства игрока
            board.set_active(pos.x2, pos.y2);

            // Ожидаем очередного выбора целевой клетки для продолжения взятия
            while (true)
            {
                auto resp = hand.get_cell();
                // Если пользователь сделал не выбор клетки — передаём ответ (QUIT / REPLAY / BACK)
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);

                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

                // Проверяем корректность выбранной целевой клетки среди допустимых продолжений
                bool is_correct = false;
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn; // запоминаем выбранный шаг продолжения
                        break;
                    }
                }
                if (!is_correct)
                    continue; // если не корректно — ждём следующего ввода

                // Применяем ход продолжения взятия: очищаем подсветки, увеличиваем счётчик взятий и двигаем фигуру
                board.clear_highlight();
                board.clear_active();
                beat_series += 1;
                board.move_piece(pos, beat_series);
                break;
            }
        }

        // Серия взятий окончена — возвращаем OK (ход игрока полностью выполнен)
        return Response::OK;
    }


  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
