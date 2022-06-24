#

PROJECT=serv
DEP = http.h action.h session.h
SRC := ssl_serv.cc session.cc http_request.cc action_routes.cc ssl_context.cc session_mgr.cc \
   lib/guid.cc lib/multipart_form.cc lib/https_transport.cc lib/http_transport.cc lib/alloc_file.cc\
   actions/static_pages.cc actions/directory.cc actions/classic_cgi.cc actions/post_form.cc \
   actions/admin/admin.cc \
   db/ipv4_log.cc

OBJDIR := obj

S_OBJ := $(SRC:%.cc=$(OBJDIR)/%.o)

#	g++ -static ssl_serv.cc -lssl -lcrypto -lpthread -lz -o serv.static
#	g++ $(SRC) -lssl -lcrypto -lpthread  -o serv

CXXFLAGS=-c -Os -Wall -Wformat-truncation=0

all: $(PROJECT)
ifneq ("$(wildcard cert/cert.pem)","")
	@echo 'Certificate found'
else
	@echo 'Generate Certificate'
	@cd cert; sh ./cert.sh; cd ..
endif

static: $(S_OBJ)
	@echo "Linking static"
	g++ $^ -lssl -lcrypto -lpthread -Os -static -lstdc++ -lz -ldl -o $@

$(PROJECT): $(S_OBJ)
	@echo Linking
	g++ $^ -lssl -lcrypto -lpthread -ldb -o $@

$(OBJDIR):
	echo "Create object directory"

#

dir_guard=@mkdir -p $(@D)

$(OBJDIR)/%.o: %.cc $(DEP)
	$(dir_guard)
	@echo -e "Compiling \033[32m$@\033[0m"
	@g++ $< $(CXXFLAGS) -o $@
	

help:
	@echo 'Command         Action'
	@echo '------------    --------------------------------------------------------'
	@echo 'make            build https-server'
	@echo 'make static     build staticaly linked https-server'
	@echo 'make clean      remove object files'
	@echo 'make docker     create alpine-based docker container with https-server'
	@echo 'make run        run https-server docker container'
	@echo 'make dclean     clean-up docker processes'

docker: static
	@echo 'Build docker container image with https-server'
	@strip static
	@docker build --rm --tag=rest-web .

run:
	@docker run -p 4433:4433  -it rest-web sh


dclean:
	PROC := $(shell docker ps -a -q -f status=exited)
	@if [ -n "$(PROC)" ]; then \
	      echo "docker removed processes $(PROC)"; \
	      docker rm $(PROC); \
	fi

clean:
	@chmod -x $(SRC)
	@find . -name "*.o" -type f -delete
	@find . -name "*.cc" -o -name "*.h" -type f -exec chmod -x {} \;

