include Make_linux.inc
#include Make_msys2.inc
#include Make_osx.inc

# Flag fundamental para habilitar o OpenMP em C++17
CXXFLAGS = -std=c++17 -fopenmp

ifdef DEBUG
    # Configurações para depuração (lento, mas com verificações de erro)
    CXXFLAGS += -g -O0 -Wall -fbounds-check -pedantic -D_GLIBCXX_DEBUG
    CXXFLAGS2 = $(CXXFLAGS)
else
    # Configurações para performance máxima (necessário para os logs de tempo)
    CXXFLAGS2 = $(CXXFLAGS) -O3 -march=native -Wall 
    CXXFLAGS += -O3 -march=native -Wall
endif

# Nome do executável final
ALL= ant_simu.exe 

default:	help

all: $(ALL)

# Remove arquivos temporários e executáveis
clean:
	@rm -fr *.o *.exe *~

# Regra de compilação para transformar .cpp em .o
.cpp.o:
	$(CXX) $(CXXFLAGS2) -c $^ -o $@	

# Linkagem final do projeto
# Importante incluir $(LIB) para SDL2 e -fopenmp para o runtime paralelo
ant_simu.exe : ant.o fractal_land.o renderer.o window.o ant_simu.o
	$(CXX) $(CXXFLAGS2) $^ -o $@ $(LIB) -fopenmp

# Comando de ajuda para o usuário
help:
	@echo "Available targets : "
	@echo "    all            : compile all executables (with OpenMP)"
	@echo "    clean          : remove objects and executables"
	@echo "Add DEBUG=yes to compile in debug mode"
	@echo "Configuration :"
	@echo "    CXX      :    $(CXX)"
	@echo "    CXXFLAGS :    $(CXXFLAGS)"

# Regra para gerar documentação se necessário
%.html: %.md
	pandoc -s --toc $< --css=./github-pandoc.css --metadata pagetitle="OS202 - TD1" -o $@