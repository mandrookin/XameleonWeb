# Акции

Акции - это обработчики HTTT запросов (GET, POST, PUT, DELETE). 
Чтобы создать акцию необходимо наследоваться от класса ```http_action_t``` и реализовать виртуальный метод ```process_req```.

Пример объявления акции, отдаюющей статические HTML страницы:  
```c++
class static_page_action : public http_action_t {
    http_response_t * process_req(https_session_t * session, url_t * url);
public:
    static_page_action(access_t rights) : http_action_t(rights) {}
};

```

Аргумент ```session``` указывает на параметры текущей сессии. Аргумент ```url``` указывает на путь и парамтры запроса.  
Примеры реализаии акций в этой папке.

Для использования акции её неоходимо зарегистрировать. Пример регистрации акции:  
```c++
    add_action_route("/", GET, new static_page_action(guest));
```

Для функционирования сервера в простейшем режиме достаточно вышеприведённой акции. В этом случае все запросы GET к серверу будут направляться 
обработчику ```process_req```.

