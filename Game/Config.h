#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

// Класс Config хранит в памяти JSON-настройки (config) и даёт удобный доступ к ним.
// При создании объект автоматически загружает настройки из файла settings.json.
class Config
{
  public:
    Config()
    {
        // При создании объекта автоматически загружаем настройки из файла.
        reload();
    }

    // reload()
    // Загружает (перезаписывает) внутреннее представление конфигурации `config`
    // из файла <project_path> + "settings.json".
    //
    // Поведение:
    // 1) Открывает файл в поток ввода std::ifstream.
    // 2) Парсит JSON с помощью nlohmann::json в поле `config`.
    // 3) Закрывает файл.

    void reload()
    {
        std::ifstream fin(project_path + "settings.json");
        fin >> config;
        fin.close();
    }

    // operator()(setting_dir, setting_name)
    // Удобный "функтор" для быстрого доступа к вложенному полю JSON-конфига.
    //
    // Пример использования:
    //   Config cfg;
    //   auto value = cfg("WindowSize", "Width"); // возвращает json-узел config["WindowSize"]["Width"]
    //
    // Особенности:
    // - Метод помечен const — не изменяет состояние объекта.
    // - Возвращаемое значение — то, что вернёт nlohmann::json при обращении по ключу:
    //   это может быть json::value_type (например число, строка, объект и т.д.).
    // - Если ключи отсутствуют, nlohmann::json создаст/вернёт `null`-узел или бросит исключение
    //   в зависимости от использования; при необходимости стоит добавить явную проверку
    //   наличия ключа (contains) и/или возвращать значение по умолчанию.
    //
    // Зачем нужен operator()?
    // - Для удобства синтаксиса: вместо cfg.config["A"]["B"] можно писать cfg("A","B").
    // - Это сокращает и делает код чище в местах частого доступа к настройкам.
    auto operator()(const string &setting_dir, const string &setting_name) const
    {
        return config[setting_dir][setting_name];
    }

  private:
    json config; // внутреннее представление загруженного JSON
};
