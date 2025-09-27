# Makefile para Encriptación Paralela
# Universidad del Valle de Guatemala

CC = g++
CFLAGS = -Wall -Wextra -std=c++11 -O2
LIBS = -lssl -lcrypto -lpthread
TARGET = encrypt_parallel
SOURCE = encrypt_parallel.cpp

# Regla principal
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

# Regla para limpiar archivos compilados
clean:
	rm -f $(TARGET) *.enc *.dec *.seq

# Regla para instalar dependencias (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y libssl-dev build-essential

# Regla para compilar en modo debug
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Regla para ejecutar tests
test: $(TARGET)
	@echo "=== Creando archivo de prueba ==="
	@echo "Este es un archivo de prueba para encriptación paralela." > test_input.txt
	@echo "Universidad del Valle de Guatemala - Ciencia de la Computación" >> test_input.txt
	@echo "Probando con múltiples líneas de texto para verificar el funcionamiento." >> test_input.txt
	@echo ""
	@echo "=== Probando encriptación con 1 hilo ==="
	@echo -e "1\ntest_input.txt\ntest_encrypted.enc\n1" | ./$(TARGET)
	@echo ""
	@echo "=== Probando encriptación con 4 hilos ==="
	@echo -e "1\ntest_input.txt\ntest_encrypted_4t.enc\n4" | ./$(TARGET)
	@echo ""
	@echo "=== Archivos generados ==="
	@ls -la test_*

help:
	@echo "Comandos disponibles:"
	@echo "  make          - Compilar el programa"
	@echo "  make clean    - Limpiar archivos compilados"
	@echo "  make debug    - Compilar en modo debug"
	@echo "  make test     - Ejecutar pruebas básicas"
	@echo "  make install-deps - Instalar dependencias"
	@echo "  make help     - Mostrar esta ayuda"
	@echo ""
	@echo "Para ejecutar manualmente:"
	@echo "  ./encrypt_parallel"

.PHONY: clean install-deps debug test help