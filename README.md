# XameleonWeb - веб сервер с поддержкой технологии REST на уровне исходного кода.

Программа является продуктом курса "Системное программирование" Ростовского-на-Дону колледжа связи и информатики  

Для сборки программы потребуются следующие инструменты разработки:  
- компилятор g++
- средство сборки make

Для сборки программы необходимо установить следующие библиотеки в версии для разработчиков:  
- openssl
- pthread
- libdb

Для статической сборки необходимо доустановить статическую версию libstdc++  
```apt-get install libstdc++-devel-static```


Чтобы отобразить возможные параметры сборки, используйте ```make help```  
Если средства сборки и библиотеки установлены корректно, выполните команду ```make```.  
В результате создастся исполняемый файл ```serv```.

Пример запуска сервера:  

```bash
$ ./serv
HTTP server listen for incoming connections on:
lo: https://127.0.0.1:4433
eth0: https://192.168.182.137:4433
```

## Запуск с правами администратора

По умолчанию при запуске от непривелигированного пользователя, сервер слушает порт https:4433. В случае старта с правами администратора, сервер открывает порты http:80 и https:443, 
затем, из соображений безопасности, сервер понижает свои привилегии до пользователя nobody и одной из групп - nodody или nogroup. В случае, если понизить уровень привилегий не удастся, сервер совершит аварийный выход.

## Переменные окружения

Путь к корневой папке сайта можно установить с помощью переменной окружения WEBPAGES. Пример:  
```bash
export WEBPAGES=/home/user/www
./serv
```

Возможно установить https порт, на котором сервер будет слушать входящие соединения. Пример установки порта  
```bash
export HTTPS_PORT=8080
./serv
```
__Предупреждение__: Для использования портов ниже 1024 необходимо иметь права администратора.

## Работа в Docker контейнере

18.06.2022 добавлена вомзожность сборки и запуска в Docker контейнере, для этого в системе, на которой производится сборка сервера, должны быть 
установлены статические версии библиотек разработчика. В этом случае сборка сервера в контейнере будет иметь следующий вид:
```bash
make static
make docker
```

И тестовый запуск сервера в контейнере.
```bash
make run
```

## Заметки

Пример запроса curl c диапазонами ```curl -v -k -r 100-200 https://localhost:4433```