# �����

����� - ��� ����������� HTTT �������� (GET, POST, PUT, DELETE). 
����� ������� �����, ���������� ������������� �� ������ ```http_action_t``` � ����������� ����������� ����� ```process_req```.

������ ���������� �����, �������� ����������� HTML ��������:  
```c++
class static_page_action : public http_action_t {
    http_response_t * process_req(https_session_t * session, url_t * url);
public:
    static_page_action(access_t rights) : http_action_t(rights) {}
};

```

�������� ```session``` ��������� �� ��������� �������� ������. �������� ```url``` ��������� �� ���� � �������� HTTP �������.  
������� ��������� ����� ��������� � ���� �����.

��� ������������� ����� � ���������� ����������������. ������ ����������� �����:  
```c++
    add_action_route("/", GET, new static_page_action(guest));
```

��� ���������������� ������� � ���������� ������ ���������� �������������� �����. � ���� ������ ��� ������� GET � ������� ����� ������������ 
����������� ```process_req```.

������������ ����� ���������� �������� ```rights``` - �� ��������� �����, ����������� ��� ��������� ���������� �����. ��������� �������� ����:  
```c++
typedef enum {
    guest,      // everybody, Not tracked
    tracked,    // everybody, Traccked
    user,       // registered user (check groups???)
    editor,     // editro rights
    admin       // admin rights
} access_t;
```
