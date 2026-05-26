# WEB-AGENT

`WEB-AGENT` на C++17 для интеграции веб-сервиса OrderDesk с локальным ПК пользователя.

## Что умеет

- работает в фоновом режиме и периодически опрашивает сервер;
- регистрируется через `POST /api/v1/agent/register` (`uid`, `hostname`, `platform`, `version`);
- получает задания через `POST /api/v1/agent/task`;
- выполняет локальные команды/программы и формирует файлы результата;
- отправляет статус и файлы через `POST /api/v1/agent/result` (`multipart/form-data`);
- поддерживает параллельное выполнение задач (`max_parallel_tasks`);
- использует retry + backoff при сетевых сбоях;
- ограничивает запуск программ через `allowed_programs`;
- поддерживает токен авторизации (`api_token`);
- поддерживает проверку TLS-сертификата (`verify_tls`, `ca_cert_path`);
- ведет лог в файл и stdout (`INFO`, `WARN`, `ERROR`, `DEBUG`);
- поддерживает mock-режим через локальные `*_response.json`.

Поддерживаемые типы задач:
- `run_command`
- `run_program`
- `generate_document`
- `collect_files`
- `upload_file`
- `print_document`

Также сохранена совместимость с legacy-типами: `CONF`, `FILE`, `TASK`, `TIMEOUT`.

## Сборка

```bash
cmake -S . -B build
cmake --build build
```

## Запуск

```bash
./build/web-agent config.json
```

## Быстрый сценарий работы

1. Агент загружает `config.json` и инициализирует логирование.
2. Регистрируется на сервере (`/api/v1/agent/register`).
3. Запускает цикл polling (`/api/v1/agent/task`).
4. Полученные задачи выполняет локально (с учетом `allowed_programs`).
5. Отправляет результат и файлы (`/api/v1/agent/result`).
6. При сетевых ошибках повторяет запросы с `retry_attempts` и `retry_backoff_sec`.

## Пример config.json

```json
{
  "uid": "codex-live-001",
  "descr": "web-agent",
  "server_url": "https://orderdesk.example.com",
  "api_token": "",
  "agent_version": "1.0.0",
  "access_code": "",
  "poll_interval": 2,
  "max_parallel_tasks": 4,
  "task_directory": "./tasks",
  "result_directory": "./results",
  "allowed_programs": ["echo", "python3", "/usr/local/bin/docgen"],
  "log_file": "./agent.log",
  "state_file": "./agent_state.json",
  "connect_timeout_sec": 5,
  "request_timeout_sec": 30,
  "retry_attempts": 3,
  "retry_backoff_sec": 10,
  "verify_tls": true,
  "ca_cert_path": "",
  "mock_api_directory": "",
  "max_cycles": 0
}
```

Ключевые поля:
- `uid` - уникальный идентификатор агента;
- `server_url` - базовый URL API сервиса;
- `api_token` - bearer-токен для запросов;
- `poll_interval` - интервал опроса задач;
- `max_parallel_tasks` - лимит параллельных задач;
- `allowed_programs` - whitelist программ/команд;
- `retry_attempts` и `retry_backoff_sec` - стратегия повторных попыток;
- `verify_tls` и `ca_cert_path` - настройки HTTPS/сертификатов;
- `mock_api_directory` - включение mock-режима;
- `max_cycles` - лимит циклов (полезно для тестов).

## API контракт

- `POST /api/v1/agent/register`  
  JSON: `uid`, `UID`, `descr`, `hostname`, `platform`, `version`

- `POST /api/v1/agent/task`  
  JSON: `UID`, `descr`, `access_code`

- `POST /api/v1/agent/result`  
  multipart-поля: `result_code`, `result`, `file1...fileN`

Во всех запросах, если задан `api_token`, добавляется заголовок:
- `Authorization: Bearer <api_token>`

## Тестирование

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Покрытие включает:
- unit-тесты (`api_types`, `config`, `task_executor`);
- интеграционные тесты (`agent + mock_api`);
- негативные и security-сценарии (недоступный сервер, allowlist).
