FROM alpine:latest
LABEL xameleon-web-version="1"
MAINTAINER Aleksey Mandrykin aka alman
COPY ./static ./static-https
RUN mkdir ./cert
COPY ./cert/*.pem ./cert/
EXPOSE 4433
RUN mkdir pages
COPY pages/ /pages/
