
## Сборка
Для сборки проекта используется `cmake`
```
cmake .
make
```
## Запуск
При запуске необходимо задать следующие параметры:
```
-h host ip address
-p host port in which server running
-d root web server directory
-s stay on foreground
-w workers count
```
Например `final -h 127.0.0.1 -p 8080 -d /tmp/server`.
Где `final` - название исполняемого файла.

После запуска, если не указан ключ `-s`, сервер демонезируется и возвращает управление.

