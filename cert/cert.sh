#!/bin/sh

# Этот скрипт создаёт пару самоподписанных ключей для отладки - приватный и публичный.
# Браузеры не доверяет таким ключам, но их достаточно для большинства функций программы и для установления зашифрованного соединения  

CN=my.test.ru
O=localhost

#openssl req -x509 -nodes -days 3650 -newkey rsa:2048 -keyout key.pem -out cert.pem -subj "/CN=$CN/O=$O"

openssl req -x509 -newkey rsa:4096 -sha256 -days 3650 -nodes \
  -keyout key.pem -out cert.pem -subj "/CN=$CN/O=$O" \
  -addext "subjectAltName=DNS:$CN,DNS:www.$CN,IP:192.168.1.193"
