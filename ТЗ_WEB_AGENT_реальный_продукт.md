# ТЕХНИЧЕСКОЕ ЗАДАНИЕ

## WEB-AGENT

## 1. Цель

Фоновый кроссплатформенный клиент, который опрашивает удалённый сервер по HTTP/HTTPS, выполняет полученные задачи (shell-команды, запуск программ, работа с файлами) и отчитывается о результате.

## 2. Платформы

- Windows 10+
- Linux (Ubuntu 20.04+, в т.ч. WSL)
- macOS 12+

## 3. Архитектура и стек

- Язык: C++17
- Сборка: CMake 3.16+
- HTTP-клиент: системный `curl` через `popen`
- JSON: собственный мини-парсер
- Логирование: собственный потокобезопасный логгер
- Тесты: Google Test (через `FetchContent`)

### Модули

| Модуль | Файлы | Назначение |
|---|---|---|
| `Config` | `src/core/config.*` | загрузка/валидация/сохранение конфига и state |
| `Logger` | `src/core/logger.*` | потокобезопасные логи в stdout и файл |
| `Agent` | `src/core/agent.*` | цикл polling, retry, обработка задач |
| `HttpClient` | `src/network/http_client.*` | HTTP/HTTPS POST JSON и multipart |
| `ApiTypes` | `src/network/api_types.*` | сериализация/парсинг API |
| `TaskExecutor` | `src/executor/task_executor.*` | выполнение задач |
| `utils` | `src/utils/*` | json, файлы, строки, платформа |

## 4. Режим работы

- Запуск из терминала: `./build/web-agent config.json`
- Работает в foreground до `Ctrl+C`
- Каждые `poll_interval_sec` секунд опрашивает сервер (по умолчанию 2 сек, максимум 3600 сек)
- Завершается автоматически после `max_cycles` опросов (`0` = бесконечно)

## 5. Конфигурация (`config.json`)

```json
{
  "uid": "agent-001",
  "descr": "web-agent",
  "server_url": "https://example.com/api",
  "access_code": "",
  "api_token": "",
  "agent_version": "1.0.0",
  "poll_interval_sec": 2,
  "max_parallel_tasks": 1,
  "max_cycles": 0,
  "task_directory": "./tasks",
  "result_directory": "./results",
  "log_file": "./agent.log",
  "state_file": "./agent_state.json",
  "connect_timeout_sec": 5,
  "request_timeout_sec": 30,
  "retry_attempts": 3,
  "retry_backoff_sec": 10,
  "verify_tls": false,
  "ca_cert_path": "",
  "allowed_programs": [],
  "mock_api_directory": ""
}
```

Если `mock_api_directory` задан — HTTP-вызовы заменяются чтением JSON из этой папки (для offline-тестов).

## 6. Протокол

Все запросы — POST с телом в JSON. Ответы — JSON. Поддерживаются два варианта эндпоинтов:

- **Legacy:** `/wa_reg/`, `/wa_task/`, `/wa_result/`
- **Новый:** `/api/v1/agent/register`, `/api/v1/agent/task`, `/api/v1/agent/result`

Агент сам выбирает вариант по подстроке `/app/webagent1/api` в `server_url`.

### 6.1 Регистрация

Если `access_code` пуст в конфиге и в `state_file` — агент шлёт регистрацию (`uid`, `descr`, `hostname`, `platform`, `agent_version`). При успехе сохраняет полученный `access_code` в `state_file`.

### 6.2 Опрос задач

Каждые `poll_interval_sec` агент шлёт `{UID, access_code, descr}`. Сервер отвечает либо `WAIT`, либо одной/несколькими задачами.

### 6.3 Отправка результата

После выполнения задачи — `multipart/form-data` с полями:
- `result_code` (int)
- `result` (JSON с `UID`, `session_id`, `message`, `files`)
- `file1`, `file2`, … — прикладываемые файлы

## 7. Типы задач

| `task_code` | Действие |
|---|---|
| `TASK` | Запуск shell-команды из `options`, захват stdout и exit code |
| `FILE` | Передача файла (имя/путь в `options`); если не найден — создаётся fallback-файл с метаданными |
| `CONF` | Сохранение `options` в `task_directory/last_task_options.txt` |
| `TIMEOUT` | Изменение `poll_interval_sec` (с верхней границей 3600 сек) |

Также распознаются строковые типы нового API (`run_command`, `run_program`, `generate_document`, `collect_files`, `upload_file`, `print_document`) — все они обрабатываются как `TASK`.

## 8. Выполнение TASK

- Команда из `options` запускается через `popen`
- На Windows — обёртка `cmd.exe /S /C "..."` (защита от перехвата `sh.exe` из Git Bash / WSL)
- Захватываются stdout + stderr и exit code
- Результат пишется в файл `task_output_<session>.txt` и прикладывается к ответу
- Если `allowed_programs` не пуст — выполняется только если имя программы в whitelist; иначе `Result code = -1`, `message = "Program is not allowed: <имя>"`

### 8.1 GUI-приложения

- **Windows**: `notepad`, `calc`, `mspaint`, `wordpad`, `write`, `explorer`, `winword` запускаются через `start ""` — приложение открывается, агент сразу получает `exit 0`
- **WSL**: те же имена автоматически переписываются в `.exe` (`notepad` → `notepad.exe`), Windows-приложение открывается через WSL interop
- **macOS** алиасы:
  - `notepad` → `open -a TextEdit`
  - `calc` → `open -a Calculator`
- Все GUI-команды запускаются в detached-режиме (`</dev/null >/dev/null 2>&1 &`), чтобы не блокировать пайп `popen`

## 9. Логирование

- Уровни: `INFO`, `WARN`, `ERROR`, `DEBUG`
- Формат: `YYYY-MM-DDTHH:MM:SS [LEVEL] сообщение`
- Параллельный вывод в stdout и в `log_file`
- Потокобезопасно (`std::mutex`)
- Каждый poll-цикл логирует `poll cycle #N (pending=K)` на уровне DEBUG для диагностики

## 10. Многопоточность

- `max_parallel_tasks = 1` (по умолчанию): задача выполняется inline в главном потоке (никакого `std::async`)
- `max_parallel_tasks > 1`: задачи выполняются в `std::async`-потоках, завершённые future очищаются в начале каждого цикла
- Доступ к `TaskExecutor` защищён `std::mutex`

## 11. Надёжность

- Повтор HTTP-запроса до `retry_attempts` раз с паузой `retry_backoff_sec` секунд
- Таймауты: `--connect-timeout` и `--max-time` у curl
- Любая ошибка одного цикла не останавливает агент — логируется и сон до следующего цикла
- `poll_interval_sec` ограничен сверху 3600 секундами (защита от runaway `TIMEOUT`)

## 12. Безопасность

- HTTPS поддерживается; проверка сертификата управляется `verify_tls` и `ca_cert_path`
- Поддерживается bearer-токен через `api_token` (заголовок `Authorization: Bearer ...`)
- `access_code` хранится отдельно в `state_file`, а не в основном конфиге
- Whitelist программ через `allowed_programs` (пустой = разрешены все)
- Все аргументы для shell-команд экранируются (`shellEscape`)

## 13. Тестирование

18 тестов на Google Test, запускаются через `ctest`:

- **Unit:** парсинг JSON, загрузка/сохранение конфига, выполнение задач (`CONF`, `FILE`, `TASK`, `TIMEOUT`), allowlist
- **Integration:** регистрация → получение задачи → отправка результата через mock-API
- **Negative:** HTTP при недоступном сервере, multipart при недоступном сервере

```bash
cd build && ctest --output-on-failure
```

## 14. Сборка и запуск

```bash
# Сборка
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j 4

# Запуск
./build/web-agent config.json

# Тесты
cd build && ctest --output-on-failure
```

Остановка — `Ctrl+C`.

## 15. Документация в репозитории

- `README.md` — сборка, запуск, описание полей конфига
- `ОТЧЕТ_ПО_РАЗРАБОТКЕ.md` — отчёт по разработке
- `mock_api/*.json` — примеры ответов сервера для offline-режима
