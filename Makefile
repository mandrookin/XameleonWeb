#

PROJECT=serv
DEP = http.h action.h session.h
SRC := ssl_serv.cc session.cc http_request.cc action_routes.cc ssl_context.cc \
   lib/guid.cc lib/multipart_form.cc \
   actions/static_pages.cc actions/directory.cc actions/classic_cgi.cc actions/post_form.cc

OBJDIR := obj

S_OBJ := $(SRC:%.cc=$(OBJDIR)/%.o)

#	g++ -static ssl_serv.cc -lssl -lcrypto -lpthread -lz -o serv.static
#	g++ $(SRC) -lssl -lcrypto -lpthread  -o serv

CXXFLAGS=-c -Wall -Wformat-truncation=0

all: $(PROJECT)
ifneq ("$(wildcard cert/cert.pem)","")
	@echo 'Certificate found'
else
	@echo 'Generate Certificate'
	@cd cert; sh ./cert.sh; cd ..
endif

$(PROJECT): $(S_OBJ)
	@echo Linking
	g++ $^ -lssl -lcrypto -lpthread -o $@

$(OBJDIR):
	echo "Create object directory"

#

dir_guard=@mkdir -p $(@D)

$(OBJDIR)/%.o: %.cc $(DEP)
	$(dir_guard)
	@echo -e "Compiling \033[32m$@\033[0m"
	@g++ $< $(CXXFLAGS) -o $@
	

clean:
	@chmod -x $(SRC)
	@find . -name "*.o" -type f -delete

test:
	@echo $(S_OBJ)
